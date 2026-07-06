/*
 * Mockingjay cache replacement policy
 * (Ishan Shah, Akanksha Jain, Calvin Lin, HPCA 2022)
 *
 * Ported to the class-based ChampSim module API from the legacy
 * CACHE::llc_* implementation.
 */

#include "mockingjay.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "champsim.h"
#include "msl/bits.h"

namespace
{
uint64_t CRC_HASH(uint64_t blockAddress)
{
  constexpr uint64_t crcPolynomial = 3988292384ULL;
  uint64_t _returnVal = blockAddress;
  for (unsigned int i = 0; i < 3; i++) {
    _returnVal = ((_returnVal & 1) == 1) ? ((_returnVal >> 1) ^ crcPolynomial) : (_returnVal >> 1);
  }
  return _returnVal;
}
} // namespace

mockingjay::mockingjay(CACHE* cache)
    : replacement(cache), NUM_SET(cache->NUM_SET), NUM_WAY(cache->NUM_WAY), LOG2_LLC_SET(static_cast<int>(champsim::msl::lg2(NUM_SET))),
      LOG2_LLC_SIZE(LOG2_LLC_SET + static_cast<int>(champsim::msl::lg2(NUM_WAY)) + static_cast<int>(LOG2_BLOCK_SIZE)),
      LOG2_SAMPLED_SETS(LOG2_LLC_SIZE - 16), INF_RD(static_cast<int>(NUM_WAY) * HISTORY - 1),
      INF_ETR((static_cast<int>(NUM_WAY) * HISTORY / GRANULARITY) - 1), MAX_RD(INF_RD - 22), SAMPLED_CACHE_TAG_BITS(31 - LOG2_LLC_SIZE),
      PC_SIGNATURE_BITS(LOG2_LLC_SIZE - 10), FLEXMIN_PENALTY(2.0 - static_cast<double>(champsim::msl::lg2(NUM_CPUS)) / 4.0),
      etr(static_cast<std::size_t>(NUM_SET * NUM_WAY), 0), etr_clock(static_cast<std::size_t>(NUM_SET), GRANULARITY),
      current_timestamp(static_cast<std::size_t>(NUM_SET), 0)
{
  for (long set = 0; set < NUM_SET; set++) {
    if (is_sampled_set(set)) {
      long modifier = 1LL << LOG2_LLC_SET;
      long limit = 1LL << LOG2_SAMPLED_CACHE_SETS;
      for (long i = 0; i < limit; i++) {
        sampled_cache[static_cast<uint32_t>(set + modifier * i)] = std::vector<SampledCacheLine>(SAMPLED_CACHE_WAYS);
      }
    }
  }
}

bool mockingjay::is_sampled_set(long set) const
{
  int mask_length = LOG2_LLC_SET - LOG2_SAMPLED_SETS;
  long mask = (1LL << mask_length) - 1;
  return (set & mask) == ((set >> (LOG2_LLC_SET - mask_length)) & mask);
}

uint64_t mockingjay::get_pc_signature(uint64_t pc, bool hit, bool prefetch, uint32_t core) const
{
  if (NUM_CPUS == 1) {
    pc = pc << 1;
    if (hit) {
      pc = pc | 1;
    }
    pc = pc << 1;
    if (prefetch) {
      pc = pc | 1;
    }
    pc = CRC_HASH(pc);
    pc = (pc << (64 - PC_SIGNATURE_BITS)) >> (64 - PC_SIGNATURE_BITS);
  } else {
    pc = pc << 1;
    if (prefetch) {
      pc = pc | 1;
    }
    pc = pc << 2;
    pc = pc | core;
    pc = CRC_HASH(pc);
    pc = (pc << (64 - PC_SIGNATURE_BITS)) >> (64 - PC_SIGNATURE_BITS);
  }
  return pc;
}

uint32_t mockingjay::get_sampled_cache_index(uint64_t full_addr) const
{
  full_addr = full_addr >> LOG2_BLOCK_SIZE;
  full_addr = (full_addr << (64 - (LOG2_SAMPLED_CACHE_SETS + LOG2_LLC_SET))) >> (64 - (LOG2_SAMPLED_CACHE_SETS + LOG2_LLC_SET));
  return static_cast<uint32_t>(full_addr);
}

uint64_t mockingjay::get_sampled_cache_tag(uint64_t x) const
{
  x >>= LOG2_LLC_SET + LOG2_BLOCK_SIZE + LOG2_SAMPLED_CACHE_SETS;
  x = (x << (64 - SAMPLED_CACHE_TAG_BITS)) >> (64 - SAMPLED_CACHE_TAG_BITS);
  return x;
}

int mockingjay::search_sampled_cache(uint64_t blockAddress, uint32_t set) const
{
  const auto& sampled_set = sampled_cache.at(set);
  for (int way = 0; way < SAMPLED_CACHE_WAYS; way++) {
    if (sampled_set[static_cast<std::size_t>(way)].valid && (sampled_set[static_cast<std::size_t>(way)].tag == blockAddress)) {
      return way;
    }
  }
  return -1;
}

void mockingjay::detrain(uint32_t set, int way)
{
  if (way < 0) {
    return;
  }
  SampledCacheLine& temp = sampled_cache.at(set)[static_cast<std::size_t>(way)];
  if (!temp.valid) {
    return;
  }

  if (rdp.count(temp.signature)) {
    rdp[temp.signature] = std::min(rdp[temp.signature] + 1, INF_RD);
  } else {
    rdp[temp.signature] = INF_RD;
  }
  temp.valid = false;
}

int mockingjay::temporal_difference(int init, int sample) const
{
  if (sample > init) {
    int diff = sample - init;
    diff = static_cast<int>(diff * TEMP_DIFFERENCE);
    diff = std::min(1, diff);
    return std::min(init + diff, INF_RD);
  } else if (sample < init) {
    int diff = init - sample;
    diff = static_cast<int>(diff * TEMP_DIFFERENCE);
    diff = std::min(1, diff);
    return std::max(init - diff, 0);
  } else {
    return init;
  }
}

int mockingjay::increment_timestamp(int input)
{
  input++;
  input = input % (1 << TIMESTAMP_BITS);
  return input;
}

int mockingjay::time_elapsed(int global, int local) const
{
  if (global >= local) {
    return global - local;
  }
  global = global + (1 << TIMESTAMP_BITS);
  return global - local;
}

long mockingjay::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                             champsim::address full_addr, access_type type)
{
  // If there's an invalid block, we don't need to evict any valid ones
  for (long way = 0; way < NUM_WAY; way++) {
    if (!current_set[way].valid) {
      return way;
    }
  }

  int max_etr = 0;
  long victim_way = 0;
  for (long way = 0; way < NUM_WAY; way++) {
    int e = etr[static_cast<std::size_t>(set * NUM_WAY + way)];
    if (std::abs(e) > max_etr || (std::abs(e) == max_etr && e < 0)) {
      max_etr = std::abs(e);
      victim_way = way;
    }
  }

  uint64_t pc_signature = get_pc_signature(ip.to<uint64_t>(), false, type == access_type::PREFETCH, triggering_cpu);
  if (type != access_type::WRITE && rdp.count(pc_signature) && (rdp[pc_signature] > MAX_RD || rdp[pc_signature] / GRANULARITY > max_etr)) {
    return NUM_WAY; // bypass
  }

  return victim_way;
}

void mockingjay::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                          champsim::address victim_addr, access_type type, uint8_t hit)
{
  // Misses are handled at fill time via replacement_cache_fill()
  if (!hit) {
    return;
  }
  update(triggering_cpu, set, way, full_addr, ip, type, true);
}

void mockingjay::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                        champsim::address victim_addr, access_type type)
{
  // way == NUM_WAY on a bypass
  update(triggering_cpu, set, way, full_addr, ip, type, false);
}

void mockingjay::update(uint32_t triggering_cpu, long set, long way, champsim::address full_addr_arg, champsim::address ip, access_type type, bool hit)
{
  const auto set_idx = static_cast<std::size_t>(set);

  if (type == access_type::WRITE) {
    if (!hit && way < NUM_WAY) {
      etr[static_cast<std::size_t>(set * NUM_WAY + way)] = -INF_ETR;
    }
    return;
  }

  uint64_t full_addr = full_addr_arg.to<uint64_t>();
  uint64_t pc = get_pc_signature(ip.to<uint64_t>(), hit, type == access_type::PREFETCH, triggering_cpu);

  if (is_sampled_set(set)) {
    uint32_t sampled_cache_index = get_sampled_cache_index(full_addr);
    uint64_t sampled_cache_tag = get_sampled_cache_tag(full_addr);
    int sampled_cache_way = search_sampled_cache(sampled_cache_tag, sampled_cache_index);
    auto& sampled_set = sampled_cache.at(sampled_cache_index);

    if (sampled_cache_way > -1) {
      uint64_t last_signature = sampled_set[static_cast<std::size_t>(sampled_cache_way)].signature;
      int last_timestamp = sampled_set[static_cast<std::size_t>(sampled_cache_way)].timestamp;
      int sample = time_elapsed(current_timestamp[set_idx], last_timestamp);

      if (sample <= INF_RD) {
        if (type == access_type::PREFETCH) {
          sample = static_cast<int>(sample * FLEXMIN_PENALTY);
        }
        if (rdp.count(last_signature)) {
          int init = rdp[last_signature];
          rdp[last_signature] = temporal_difference(init, sample);
        } else {
          rdp[last_signature] = sample;
        }

        sampled_set[static_cast<std::size_t>(sampled_cache_way)].valid = false;
      }
    }

    int lru_way = -1;
    int lru_rd = -1;
    for (int w = 0; w < SAMPLED_CACHE_WAYS; w++) {
      if (!sampled_set[static_cast<std::size_t>(w)].valid) {
        lru_way = w;
        lru_rd = INF_RD + 1;
        continue;
      }

      int last_timestamp = sampled_set[static_cast<std::size_t>(w)].timestamp;
      int sample = time_elapsed(current_timestamp[set_idx], last_timestamp);
      if (sample > INF_RD) {
        lru_way = w;
        lru_rd = INF_RD + 1;
        detrain(sampled_cache_index, w);
      } else if (sample > lru_rd) {
        lru_way = w;
        lru_rd = sample;
      }
    }
    detrain(sampled_cache_index, lru_way);

    for (auto& line : sampled_set) {
      if (!line.valid) {
        line.valid = true;
        line.signature = pc;
        line.tag = sampled_cache_tag;
        line.timestamp = current_timestamp[set_idx];
        break;
      }
    }

    current_timestamp[set_idx] = increment_timestamp(current_timestamp[set_idx]);
  }

  if (etr_clock[set_idx] == GRANULARITY) {
    for (long w = 0; w < NUM_WAY; w++) {
      if (w != way && std::abs(etr[static_cast<std::size_t>(set * NUM_WAY + w)]) < INF_ETR) {
        etr[static_cast<std::size_t>(set * NUM_WAY + w)]--;
      }
    }
    etr_clock[set_idx] = 0;
  }
  etr_clock[set_idx]++;

  if (way < NUM_WAY) {
    const auto way_idx = static_cast<std::size_t>(set * NUM_WAY + way);
    if (!rdp.count(pc)) {
      etr[way_idx] = (NUM_CPUS == 1) ? 0 : INF_ETR;
    } else {
      if (rdp[pc] > MAX_RD) {
        etr[way_idx] = INF_ETR;
      } else {
        etr[way_idx] = rdp[pc] / GRANULARITY;
      }
    }
  }
}
