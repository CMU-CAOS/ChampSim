#ifndef REPLACEMENT_GLIDER_H
#define REPLACEMENT_GLIDER_H

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

#include "cache.h"
#include "modules.h"

struct glider : public champsim::modules::replacement {
  // RRPV constants (3-level: 0=friendly, 2=medium, 7=averse)
  static constexpr int MAXRRPV = 7;
  static constexpr int RRPV_FRIENDLY = 0;
  static constexpr int RRPV_MEDIUM = 2;
  static constexpr int RRPV_AVERSE = 7;

  // ISVM predictor constants
  static constexpr std::size_t ISVM_ENTRIES = 2048;   // PC-indexed rows
  static constexpr std::size_t ISVM_WEIGHTS = 16;     // weights per row (4-bit hash -> 16 buckets)
  static constexpr int WEIGHT_MAX = 127;
  static constexpr int WEIGHT_MIN = -128;
  static constexpr int PRED_THRESHOLD = 60;            // friendly if sum >= 60
  static constexpr int TRAIN_THRESHOLD = 100;          // only train if |sum| < this (perceptron margin)

  // PCHR constants
  static constexpr std::size_t PCHR_SIZE = 5;          // 5 unique recent PCs

  // OPTgen constants (same as Hawkeye)
  static constexpr std::size_t OPTGEN_SIZE = 128;
  static constexpr std::size_t SAMPLER_HIST = 8;

  // OPTgen: models Belady's optimal per sampled set
  // Faithful to CRC-2 reference (Sacusa/ChampSim optgen.h)
  struct OPTgen {
    std::vector<unsigned int> liveness;
    std::size_t cache_size = 0;

    void init(std::size_t sz) {
      cache_size = sz;
      liveness.assign(OPTGEN_SIZE, 0);
    }

    // Check [last_quanta, curr_quanta) and update liveness if cacheable
    bool should_cache(uint64_t curr_quanta, uint64_t last_quanta) {
      bool is_cache = true;
      unsigned int i = static_cast<unsigned int>(last_quanta);
      while (i != static_cast<unsigned int>(curr_quanta)) {
        if (liveness[i] >= cache_size) {
          is_cache = false;
          break;
        }
        i = (i + 1) % OPTGEN_SIZE;
      }
      if (is_cache) {
        i = static_cast<unsigned int>(last_quanta);
        while (i != static_cast<unsigned int>(curr_quanta)) {
          liveness[i]++;
          i = (i + 1) % OPTGEN_SIZE;
        }
      }
      return is_cache;
    }

    // Reset liveness at the current quanta
    void add_access(uint64_t curr_quanta) {
      liveness[curr_quanta % OPTGEN_SIZE] = 0;
    }
  };

  struct SamplerEntry {
    bool valid = false;
    champsim::address address{};  // full address for matching
    uint64_t pc = 0;
    uint32_t timestamp = 0;
    bool prefetch = false;
    uint64_t lru = 0;
    std::array<uint64_t, PCHR_SIZE> saved_pchr{};  // PCHR snapshot at time of access
    std::size_t saved_pchr_size = 0;
  };

  long NUM_SET, NUM_WAY;
  uint64_t sampler_access_count = 0;

  // Per cache line state
  std::vector<int> rrpv;

  // Sampler and OPTgen (per CPU, indexed like SHIP)
  std::vector<OPTgen> optgen;
  std::vector<std::vector<SamplerEntry>> sampler;
  std::vector<uint64_t> set_timer;

  // ISVM predictor (per CPU): isvm[cpu][pc_index][weight_index]
  std::vector<std::array<std::array<int, ISVM_WEIGHTS>, ISVM_ENTRIES>> isvm;

  // PC History Register (global)
  std::deque<uint64_t> pchr;

  explicit glider(CACHE* cache);

  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set,
                   const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type);
  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way,
                              champsim::address full_addr, champsim::address ip,
                              champsim::address victim_addr, access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way,
                                champsim::address full_addr, champsim::address ip,
                                champsim::address victim_addr, access_type type, uint8_t hit);

  static uint64_t crc_hash(uint64_t key);

  // --- Sampled-set scheme. Ported to this ChampSim tree, whose replacement
  //     base class lacks the fork's built-in get_num_sampled_sets /
  //     get_set_sample_rate / get_set_sample_category helpers. Classic
  //     Hawkeye/Glider samples 64 uniformly-spaced sets; s_idx = set/rate in
  //     0..63 maps a sampled set to its contiguous sampler slot.
  static constexpr long NUM_SAMPLED_SETS = 64;
  long get_num_sampled_sets() const { return NUM_SAMPLED_SETS; }
  long get_set_sample_rate() const { return NUM_SET / NUM_SAMPLED_SETS; }
  long get_set_sample_category(long set) const { return (set % get_set_sample_rate() == 0) ? 0 : 1; }

  bool is_sampled(long set) { return get_set_sample_category(set) == 0; }

  // ISVM helpers
  int isvm_predict(uint32_t cpu, uint64_t pc);
  void isvm_train_with_pchr(uint32_t cpu, uint64_t pc, bool cache_friendly,
                            const uint64_t* pchr_data, std::size_t pchr_len);
  void pchr_update(uint64_t pc);
  void save_pchr(SamplerEntry& entry);

private:
  int& get_rrpv(long set, long way);
};

#endif
