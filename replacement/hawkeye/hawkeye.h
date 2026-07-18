#ifndef REPLACEMENT_HAWKEYE_H
#define REPLACEMENT_HAWKEYE_H

// Hawkeye [Jain and Lin, ISCA'16], ported from the CRC-2 llc_repl API to the
// class-based champsim::modules::replacement API. The algorithm is unchanged;
// only the interface and the (formerly global) state moved into the class.

#include <cassert>
#include <cstdint>
#include <map>
#include <vector>

#include "cache.h"
#include "modules.h"

// Constants that were #defines in the original hawkeye.llc_repl. Kept as
// macros so the helper headers (optgen.h, hawkeye_predictor.h) stay verbatim.
#define NUM_CORE 1
#define maxRRPV 7
#define TIMER_SIZE 1024
#define MAX_SHCT 31
#define SHCT_SIZE_BITS 11
#define SHCT_SIZE (1 << SHCT_SIZE_BITS)
#define OPTGEN_VECTOR_SIZE 128
#define SAMPLED_CACHE_SIZE 2800
#define SAMPLER_WAYS 8
#define SAMPLER_SETS (SAMPLED_CACHE_SIZE / SAMPLER_WAYS)

#include "hawkeye_predictor.h" // CRC(), HAWKEYE_PC_PREDICTOR   (uses MAX_SHCT, SHCT_SIZE)
#include "optgen.h"            // ADDR_INFO, OPTgen             (uses OPTGEN_VECTOR_SIZE)

class hawkeye : public champsim::modules::replacement
{
  long NUM_SET;
  long NUM_WAY;
  int LOG2_LLC_SETS;

  // State that was file-scope global arrays in the original, now per-instance.
  std::vector<uint32_t> rrpv;             // [set*way]
  std::vector<uint64_t> perset_mytimer;   // [set]
  std::vector<uint64_t> signatures;       // [set*way]
  std::vector<uint8_t> prefetched;        // [set*way]
  std::vector<OPTgen> perset_optgen;      // [set]
  std::vector<std::map<uint64_t, ADDR_INFO>> addr_history; // [SAMPLER_SETS]
  HAWKEYE_PC_PREDICTOR* demand_predictor = nullptr;
  HAWKEYE_PC_PREDICTOR* prefetch_predictor = nullptr;

  bool is_sampled_set(long set) const;
  void replace_addr_history_element(unsigned int sampler_set);
  void update_addr_history_lru(unsigned int sampler_set, unsigned int curr_lru);
  // Common hit/fill handler (the body of the original llc_update_replacement_state).
  void update(uint32_t cpu, long set, long way, uint64_t paddr, uint64_t PC, access_type type, uint8_t hit);

public:
  explicit hawkeye(CACHE* cache);

  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, uint8_t hit);
  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type);
  void replacement_final_stats();
};

#endif
