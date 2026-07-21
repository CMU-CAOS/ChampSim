#include "ipcp_L2.h"

#include <iostream> // ported: Pythia got cout/endl transitively via its headers
using namespace std;

namespace knob
{

}

void ipcp_L2::init_knobs()
{

}

void ipcp_L2::init_stats()
{

}

void ipcp_L2::prefetcher_initialize()
{
   init_knobs();
   init_stats();
   print_config();
}

void ipcp_L2::print_config()
{
   cout << "IPCP_AT_L2_CONFIG" << endl
   << "NUM_IP_TABLE_L2_ENTRIES" << NUM_IP_TABLE_L2_ENTRIES << endl
   << "NUM_GHB_ENTRIES" << NUM_GHB_ENTRIES << endl
   << "NUM_IP_INDEX_BITS" << NUM_IP_INDEX_BITS << endl
   << "NUM_IP_TAG_BITS" << NUM_IP_TAG_BITS << endl
   << "S_TYPE" << S_TYPE << endl
   << "CS_TYPE" << CS_TYPE << endl
   << "CPLX_TYPE" << CPLX_TYPE << endl
   << "NL_TYPE" << NL_TYPE << endl
   << endl;
}

int ipcp_L2::decode_stride(uint32_t metadata)
{
    int stride=0;
    if(metadata & 0b1000000)
        stride = -1*(metadata & 0b111111);
    else
        stride = metadata & 0b111111;

    return stride;
}

uint32_t ipcp_L2::prefetcher_cache_operate(champsim::address addr_, champsim::address ip_, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                           uint32_t metadata_in)
{
   uint64_t addr = addr_.to<uint64_t>();
   uint64_t ip = ip_.to<uint64_t>();
   uint32_t cpu = 0; // single-core eval (Pythia used m_parent_cache->cpu)
   uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;
   int prefetch_degree = 0;
   int64_t stride = decode_stride(metadata_in);
   uint32_t pref_type = metadata_in & 0xF00;
   uint16_t ip_tag = (ip >> NUM_IP_INDEX_BITS) & ((1 << NUM_IP_TAG_BITS)-1);

   if(NUM_CPUS == 1){
     if (intern_->get_mshr_occupancy() < intern_->get_mshr_size()/2)
      prefetch_degree = 4;
     else
      prefetch_degree = 3;
   } else {                                    // tightening the degree for multi-core
      prefetch_degree = 2;
   }

   // calculate the index bit
   int index = ip & ((1 << NUM_IP_INDEX_BITS)-1);
   if(trackers[cpu][index].ip_tag != ip_tag){              // new/conflict IP
      if(trackers[cpu][index].ip_valid == 0){             // if valid bit is zero, update with latest IP info
      trackers[cpu][index].ip_tag = ip_tag;
      trackers[cpu][index].pref_type = pref_type;
      trackers[cpu][index].stride = stride;
   } else {
      trackers[cpu][index].ip_valid = 0;                  // otherwise, reset valid bit and leave the previous IP as it is
   }

      // issue a next line prefetch upon encountering new IP
      uint64_t pf_address = ((addr>>LOG2_BLOCK_SIZE)+1) << LOG2_BLOCK_SIZE;
      prefetch_line(champsim::address{pf_address}, true, 0);
      SIG_DP(cout << "1, ");
      // return metadata_in;
   }
   else {                                                  // if same IP encountered, set valid bit
      trackers[cpu][index].ip_valid = 1;
   }

   // update the IP table upon receiving metadata from prefetch
   if(type == access_type::PREFETCH){
      trackers[cpu][index].pref_type = pref_type;
      trackers[cpu][index].stride = stride;
      spec_nl_l2[cpu] = metadata_in & 0x1000;
   }

   SIG_DP(
   cout << ip << ", " << cache_hit << ", " << cl_addr << ", ";
   cout << ", " << stride << "; ";
   );


   // we are prefetching only for GS, CS and NL classes
   if(trackers[cpu][index].stride != 0){
      if(trackers[cpu][index].pref_type == 0x100 || trackers[cpu][index].pref_type == 0x200){      // GS or CS class
         if(trackers[cpu][index].pref_type == 0x100)
          if(NUM_CPUS == 1)
             prefetch_degree = 4;
           for (int i=0; i<prefetch_degree; i++) {
               uint64_t pf_address = (cl_addr + (trackers[cpu][index].stride*(i+1))) << LOG2_BLOCK_SIZE;

               // Check if prefetch address is in same 4 KB page
               if ((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE))
                   break;

               prefetch_line(champsim::address{pf_address}, true, 0);
               SIG_DP(cout << trackers[cpu][index].stride << ", ");
           }
      }
      else if(trackers[cpu][index].pref_type == 0x400 && spec_nl_l2[cpu] > 0){
      uint64_t pf_address = ((addr>>LOG2_BLOCK_SIZE)+1) << LOG2_BLOCK_SIZE;
      prefetch_line(champsim::address{pf_address}, true, 0);
      SIG_DP(cout << "1;");
      }
   }

   SIG_DP(cout << endl);
   return metadata_in;
}

uint32_t ipcp_L2::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
   return metadata_in;
}

void ipcp_L2::prefetcher_final_stats()
{

}
