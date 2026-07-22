#ifndef IPCP_L2_H
#define IPCP_L2_H

/*****************************************************
Code taken from
Samuel Pakalapati - pakalapatisamuel@gmail.com
Biswabandan Panda - biswap@cse.iitk.ac.in
******************************************************/

#include "cache.h"
#include "champsim.h"
#include "modules.h"
#include "ipcp_vars.h"

// Ported: Pythia's cache.h supplied FILL_L2. ipcp_L2 sits at L2 -> fill this level.
#ifndef FILL_L2
#define FILL_L2 2
#endif

class IP_TRACKER
{
  public:
    uint64_t ip_tag;
    uint16_t ip_valid;
    uint32_t pref_type;                                     // prefetch class type
    int stride;                                             // last stride sent by metadata

    IP_TRACKER () {
        ip_tag = 0;
        ip_valid = 0;
        pref_type = 0;
        stride = 0;
    };
};

class ipcp_L2 : public champsim::modules::prefetcher
{
public:
   uint32_t spec_nl_l2[NUM_CPUS] = {0};
   IP_TRACKER trackers[NUM_CPUS][NUM_IP_TABLE_L2_ENTRIES];

private:
   void init_knobs();
   void init_stats();
   int decode_stride(uint32_t metadata);

public:
   using prefetcher::prefetcher;
	void prefetcher_initialize();
	// Pythia's real entry was the metadata_in overload (L2 IPCP consumes the
	// metadata L1 IPCP encoded); the modern hook supplies metadata_in directly.
	uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
	                                  uint32_t metadata_in);
	uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
	void prefetcher_final_stats();
	void print_config();
};

#endif /* IPCP_L2_H */
