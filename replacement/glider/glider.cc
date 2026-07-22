#include "glider.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

#include "champsim.h"

uint64_t glider::crc_hash(uint64_t key) {
  static constexpr uint64_t crcPolynomial = 0xEDB88320ULL;
  uint64_t val = key;
  for (unsigned int i = 0; i < 32; i++)
    val = ((val & 1) == 1) ? ((val >> 1) ^ crcPolynomial) : (val >> 1);
  return val;
}

int& glider::get_rrpv(long set, long way) {
  return rrpv.at(static_cast<std::size_t>(set * NUM_WAY + way));
}

// Update PCHR: add PC if not already present; if present, move to front
void glider::pchr_update(uint64_t pc) {
  auto it = std::find(pchr.begin(), pchr.end(), pc);
  if (it != pchr.end())
    pchr.erase(it);
  pchr.push_front(pc);
  while (pchr.size() > PCHR_SIZE)
    pchr.pop_back();
}

// Save current PCHR state into a sampler entry
void glider::save_pchr(SamplerEntry& entry) {
  entry.saved_pchr_size = std::min(pchr.size(), PCHR_SIZE);
  for (std::size_t i = 0; i < entry.saved_pchr_size; i++)
    entry.saved_pchr[i] = pchr[i];
}

// ISVM prediction using current PCHR (for fill-time decisions)
int glider::isvm_predict(uint32_t cpu, uint64_t pc) {
  std::size_t row = crc_hash(pc) % ISVM_ENTRIES;
  int sum = 0;
  for (auto& pchr_pc : pchr) {
    std::size_t col = crc_hash(pchr_pc) & 0xF;
    sum += isvm[cpu][row][col];
  }
  return sum;
}

// ISVM training using a SAVED PCHR snapshot (for correct context)
// Only trains if prediction is not already confident (perceptron margin)
void glider::isvm_train_with_pchr(uint32_t cpu, uint64_t pc, bool cache_friendly,
                                  const uint64_t* pchr_data, std::size_t pchr_len) {
  std::size_t row = crc_hash(pc) % ISVM_ENTRIES;

  // Compute current prediction to check training threshold
  int sum = 0;
  for (std::size_t i = 0; i < pchr_len; i++) {
    std::size_t col = crc_hash(pchr_data[i]) & 0xF;
    sum += isvm[cpu][row][col];
  }

  // Only train if prediction is wrong or not confident enough
  bool prediction_correct = (cache_friendly && sum >= PRED_THRESHOLD) ||
                            (!cache_friendly && sum < 0);
  if (prediction_correct && std::abs(sum) >= TRAIN_THRESHOLD)
    return;

  for (std::size_t i = 0; i < pchr_len; i++) {
    std::size_t col = crc_hash(pchr_data[i]) & 0xF;
    if (cache_friendly) {
      if (isvm[cpu][row][col] < WEIGHT_MAX)
        isvm[cpu][row][col]++;
    } else {
      if (isvm[cpu][row][col] > WEIGHT_MIN)
        isvm[cpu][row][col]--;
    }
  }
}

glider::glider(CACHE* cache)
    : replacement(cache),
      NUM_SET(cache->NUM_SET),
      NUM_WAY(cache->NUM_WAY),
      rrpv(static_cast<std::size_t>(NUM_SET * NUM_WAY), MAXRRPV)
{
  auto num_sampled = static_cast<std::size_t>(get_num_sampled_sets());

  optgen.resize(static_cast<std::size_t>(NUM_CPUS) * num_sampled);
  sampler.resize(static_cast<std::size_t>(NUM_CPUS) * num_sampled);
  set_timer.resize(static_cast<std::size_t>(NUM_CPUS) * num_sampled, 0);

  for (std::size_t i = 0; i < static_cast<std::size_t>(NUM_CPUS) * num_sampled; i++) {
    optgen[i].init(static_cast<std::size_t>(NUM_WAY - 2));
    sampler[i].resize(SAMPLER_HIST);
  }

  // ISVM predictor: all weights initialized to 0
  isvm.resize(NUM_CPUS);
  for (auto& cpu_isvm : isvm)
    for (auto& row : cpu_isvm)
      for (auto& w : row)
        w = 0;
}

long glider::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set,
                         const champsim::cache_block* current_set, champsim::address ip,
                         champsim::address full_addr, access_type type) {
  for (long way = 0; way < NUM_WAY; way++) {
    if (!current_set[way].valid)
      return way;
  }

  auto begin = std::next(std::begin(rrpv), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);
  auto victim = std::max_element(begin, end);

  if (*victim < MAXRRPV) {
    int increment = MAXRRPV - *victim;
    for (auto it = begin; it != end; ++it)
      *it += increment;
  }

  // No negative training on eviction from real cache — only through sampler
  return std::distance(begin, victim);
}

void glider::update_replacement_state(uint32_t triggering_cpu, long set, long way,
                                      champsim::address full_addr, champsim::address ip,
                                      champsim::address victim_addr, access_type type,
                                      uint8_t hit) {
  using namespace champsim::data::data_literals;

  uint64_t pc = ip.to<uint64_t>();

  // Update PCHR on every LLC access
  pchr_update(pc);

  // Sampler training (same mapping as SHIP)
  if (is_sampled(set)) {
    auto s_idx = static_cast<std::size_t>(set / get_set_sample_rate());
    std::size_t sampler_idx = triggering_cpu * static_cast<std::size_t>(get_num_sampled_sets()) + s_idx;
    auto& s_set = sampler[sampler_idx];
    auto& timer = set_timer[sampler_idx];
    auto& og = optgen[sampler_idx];

    // Match using full address (like SHIP)
    auto shamt = champsim::data::bits{champsim::lg2(get_num_sampled_sets()) + champsim::lg2(NUM_WAY)};
    auto match = std::find_if(s_set.begin(), s_set.end(),
                              [addr = full_addr, shamt](const SamplerEntry& e) {
                                return e.valid && e.address.slice_upper(shamt) == addr.slice_upper(shamt);
                              });

    uint32_t curr_timer = static_cast<uint32_t>(timer % OPTGEN_SIZE);

    if (match != s_set.end()) {
      // Sampler hit: check OPTgen and train ISVM with SAVED PCHR
      uint32_t prev_timer = match->timestamp;
      bool opt_hit = og.should_cache(curr_timer, prev_timer);
      og.add_access(curr_timer);

      // Train using the PCHR that was saved when this entry was last accessed
      isvm_train_with_pchr(triggering_cpu, match->pc, opt_hit,
                           match->saved_pchr.data(), match->saved_pchr_size);

      // Update entry with current state
      match->timestamp = curr_timer;
      match->pc = pc;
      match->address = full_addr;
      match->prefetch = (access_type{type} == access_type::PREFETCH);
      match->lru = sampler_access_count++;
      save_pchr(*match);  // Save current PCHR for future training
    } else {
      // Sampler miss: evict LRU, train negatively with saved PCHR
      auto lru_entry = std::min_element(s_set.begin(), s_set.end(),
                                        [](const SamplerEntry& a, const SamplerEntry& b) {
                                          if (!a.valid) return true;
                                          if (!b.valid) return false;
                                          return a.lru < b.lru;
                                        });

      if (lru_entry->valid) {
        isvm_train_with_pchr(triggering_cpu, lru_entry->pc, false,
                             lru_entry->saved_pchr.data(), lru_entry->saved_pchr_size);
      }

      lru_entry->valid = true;
      lru_entry->address = full_addr;
      lru_entry->pc = pc;
      lru_entry->timestamp = curr_timer;
      lru_entry->prefetch = (access_type{type} == access_type::PREFETCH);
      lru_entry->lru = sampler_access_count++;
      save_pchr(*lru_entry);  // Save current PCHR
      og.add_access(curr_timer);
    }

    timer++;
  }

  // On cache hit, promote to friendly
  if (hit)
    get_rrpv(set, way) = RRPV_FRIENDLY;
}

void glider::replacement_cache_fill(uint32_t triggering_cpu, long set, long way,
                                    champsim::address full_addr, champsim::address ip,
                                    champsim::address victim_addr, access_type type) {
  if (access_type{type} == access_type::WRITE) {
    get_rrpv(set, way) = MAXRRPV - 1;
    return;
  }

  uint64_t pc = ip.to<uint64_t>();
  int prediction = isvm_predict(triggering_cpu, pc);

  // 3-level RRPV assignment
  if (prediction >= PRED_THRESHOLD) {
    // Cache-friendly: RRPV = 0, age other lines
    get_rrpv(set, way) = RRPV_FRIENDLY;

    auto begin = std::next(std::begin(rrpv), set * NUM_WAY);
    auto end = std::next(begin, NUM_WAY);
    bool saturated = false;
    for (auto it = begin; it != end; ++it) {
      if (std::distance(begin, it) != way && *it == MAXRRPV - 1)
        saturated = true;
    }
    if (!saturated) {
      for (auto it = begin; it != end; ++it) {
        if (std::distance(begin, it) != way && *it < MAXRRPV - 1)
          (*it)++;
      }
    }
  } else if (prediction < 0) {
    // Cache-averse
    get_rrpv(set, way) = RRPV_AVERSE;
  } else {
    // Medium priority
    get_rrpv(set, way) = RRPV_MEDIUM;
  }
}
