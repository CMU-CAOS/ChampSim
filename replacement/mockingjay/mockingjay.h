#ifndef REPLACEMENT_MOCKINGJAY_H
#define REPLACEMENT_MOCKINGJAY_H

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "cache.h"
#include "modules.h"

class mockingjay : public champsim::modules::replacement
{
  struct SampledCacheLine {
    bool valid = false;
    uint64_t tag = 0;
    uint64_t signature = 0;
    int timestamp = 0;
  };

  long NUM_SET;
  long NUM_WAY;

  // Derived parameters (computed at construction; the reference implementation
  // derived these at compile time from the LLC_SET/LLC_WAY/LOG2_BLOCK_SIZE macros)
  int LOG2_LLC_SET;
  int LOG2_LLC_SIZE;
  int LOG2_SAMPLED_SETS;
  int INF_RD;
  int INF_ETR;
  int MAX_RD;
  int SAMPLED_CACHE_TAG_BITS;
  int PC_SIGNATURE_BITS;
  double FLEXMIN_PENALTY;

  static constexpr int HISTORY = 8;
  static constexpr int GRANULARITY = 8;
  static constexpr int SAMPLED_CACHE_WAYS = 5;
  static constexpr int LOG2_SAMPLED_CACHE_SETS = 4;
  static constexpr int TIMESTAMP_BITS = 8;
  static constexpr double TEMP_DIFFERENCE = 1.0 / 16.0;

  std::vector<int> etr;               // [set][way], flattened
  std::vector<int> etr_clock;         // [set]
  std::vector<int> current_timestamp; // [set]
  std::unordered_map<uint64_t, int> rdp;
  std::unordered_map<uint32_t, std::vector<SampledCacheLine>> sampled_cache;

  bool is_sampled_set(long set) const;
  uint64_t get_pc_signature(uint64_t pc, bool hit, bool prefetch, uint32_t core) const;
  uint32_t get_sampled_cache_index(uint64_t full_addr) const;
  uint64_t get_sampled_cache_tag(uint64_t x) const;
  int search_sampled_cache(uint64_t blockAddress, uint32_t set) const;
  void detrain(uint32_t set, int way);
  int temporal_difference(int init, int sample) const;
  int time_elapsed(int global, int local) const;
  static int increment_timestamp(int input);

  // Common hit/fill update, equivalent to the reference llc_update_replacement_state
  void update(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, access_type type, bool hit);

public:
  explicit mockingjay(CACHE* cache);

  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, uint8_t hit);
  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type);
};

#endif
