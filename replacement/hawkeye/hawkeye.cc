//Hawkeye Cache Replacement Tool v2.0
//UT AUSTIN RESEARCH LICENSE (SOURCE CODE)
//The University of Texas at Austin has developed certain software and documentation that it desires to
//make available without charge to anyone for academic, research, experimental or personal use.
//This license is designed to guarantee freedom to use the software for these purposes. If you wish to
//distribute or make other use of the software, you may purchase a license to do so from the University of
//Texas.
///////////////////////////////////////////////
//                                            //
//     Hawkeye [Jain and Lin, ISCA' 16]       //
//     Akanksha Jain, akanksha@cs.utexas.edu  //
//                                            //
///////////////////////////////////////////////

// Source code for configs 1 and 2

#include "hawkeye.h"

#include <cmath>
#include <iostream>

#include "champsim.h"
#include "msl/bits.h"

#define bitmask(l) (((l) == 64) ? (unsigned long long)(-1LL) : ((1LL << (l)) - 1LL))
#define bits(x, i, l) (((x) >> (i)) & bitmask(l))
// Sample 64 sets: low 6 index bits equal the 6 bits above them.
bool hawkeye::is_sampled_set(long set) const
{
  return bits(set, 0, 6) == bits(set, (LOG2_LLC_SETS - 6), 6);
}

// initialize replacement state (was CACHE::llc_initialize_replacement)
hawkeye::hawkeye(CACHE* cache)
    : replacement(cache), NUM_SET(cache->NUM_SET), NUM_WAY(cache->NUM_WAY), LOG2_LLC_SETS(static_cast<int>(champsim::msl::lg2(cache->NUM_SET))),
      rrpv(static_cast<std::size_t>(NUM_SET * NUM_WAY), maxRRPV), perset_mytimer(static_cast<std::size_t>(NUM_SET), 0),
      signatures(static_cast<std::size_t>(NUM_SET * NUM_WAY), 0), prefetched(static_cast<std::size_t>(NUM_SET * NUM_WAY), 0),
      perset_optgen(static_cast<std::size_t>(NUM_SET))
{
  for (long i = 0; i < NUM_SET; i++)
    perset_optgen[static_cast<std::size_t>(i)].init(NUM_WAY - 2);

  addr_history.resize(SAMPLER_SETS);

  demand_predictor = new HAWKEYE_PC_PREDICTOR();
  prefetch_predictor = new HAWKEYE_PC_PREDICTOR();

  std::cout << "Initialize Hawkeye state" << std::endl;
}

// find replacement victim
long hawkeye::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                          champsim::address full_addr, access_type type)
{
    // look for the maxRRPV line
    for (long i=0; i<NUM_WAY; i++)
        if (rrpv[static_cast<std::size_t>(set*NUM_WAY+i)] == maxRRPV)
            return i;

    //If we cannot find a cache-averse line, we evict the oldest cache-friendly line
    uint32_t max_rrip = 0;
    long lru_victim = -1;
    for (long i=0; i<NUM_WAY; i++)
    {
        if (rrpv[static_cast<std::size_t>(set*NUM_WAY+i)] >= max_rrip)
        {
            max_rrip = rrpv[static_cast<std::size_t>(set*NUM_WAY+i)];
            lru_victim = i;
        }
    }

    assert (lru_victim != -1);
    //The predictor is trained negatively on LRU evictions
    if( is_sampled_set(set) )
    {
        auto v = static_cast<std::size_t>(set*NUM_WAY+lru_victim);
        if(prefetched[v])
            prefetch_predictor->decrement(signatures[v]);
        else
            demand_predictor->decrement(signatures[v]);
    }
    return lru_victim;
}

void hawkeye::replace_addr_history_element(unsigned int sampler_set)
{
    uint64_t lru_addr = 0;
    
    for(map<uint64_t, ADDR_INFO>::iterator it=addr_history[sampler_set].begin(); it != addr_history[sampler_set].end(); it++)
    {
   //     uint64_t timer = (it->second).last_quanta;

        if((it->second).lru == (SAMPLER_WAYS-1))
        {
            //lru_time =  (it->second).last_quanta;
            lru_addr = it->first;
            break;
        }
    }

    addr_history[sampler_set].erase(lru_addr);
}

void hawkeye::update_addr_history_lru(unsigned int sampler_set, unsigned int curr_lru)
{
    for(map<uint64_t, ADDR_INFO>::iterator it=addr_history[sampler_set].begin(); it != addr_history[sampler_set].end(); it++)
    {
        if((it->second).lru < curr_lru)
        {
            (it->second).lru++;
            assert((it->second).lru < SAMPLER_WAYS); 
        }
    }
}


// New-API entry points. In this ChampSim, hits arrive via
// update_replacement_state and fills (misses/writebacks) via
// replacement_cache_fill; both feed the common update() below, which is the
// body of the original llc_update_replacement_state.
void hawkeye::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                       champsim::address victim_addr, access_type type, uint8_t hit)
{
    if (!hit)
        return; // misses are handled at fill time via replacement_cache_fill()
    update(triggering_cpu, set, way, full_addr.to<uint64_t>(), ip.to<uint64_t>(), type, hit);
}

void hawkeye::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                     champsim::address victim_addr, access_type type)
{
    update(triggering_cpu, set, way, full_addr.to<uint64_t>(), ip.to<uint64_t>(), type, 0);
}

// called on every cache hit and cache fill (body of the original llc_update_replacement_state)
void hawkeye::update (uint32_t cpu, long set, long way, uint64_t paddr, uint64_t PC, access_type type, uint8_t hit)
{
    paddr = (paddr >> 6) << 6;

    if(type == access_type::PREFETCH)
    {
        if (!hit)
            prefetched[static_cast<std::size_t>(set*NUM_WAY+way)] = true;
    }
    else
        prefetched[static_cast<std::size_t>(set*NUM_WAY+way)] = false;

    //Ignore writebacks
    if (type == access_type::WRITE)
        return;


    //If we are sampling, OPTgen will only see accesses from sampled sets
    if(is_sampled_set(set))
    {
        //The current timestep 
        uint64_t curr_quanta = perset_mytimer[set] % OPTGEN_VECTOR_SIZE;

        uint32_t sampler_set = (paddr >> 6) % SAMPLER_SETS; 
        uint64_t sampler_tag = CRC(paddr >> 12) % 256;
        assert(sampler_set < SAMPLER_SETS);

        // This line has been used before. Since the right end of a usage interval is always 
        //a demand, ignore prefetches
        if((addr_history[sampler_set].find(sampler_tag) != addr_history[sampler_set].end()) && (type != access_type::PREFETCH))
        {
            unsigned int curr_timer = perset_mytimer[set];
            if(curr_timer < addr_history[sampler_set][sampler_tag].last_quanta)
               curr_timer = curr_timer + TIMER_SIZE;
            bool wrap =  ((curr_timer - addr_history[sampler_set][sampler_tag].last_quanta) > OPTGEN_VECTOR_SIZE);
            uint64_t last_quanta = addr_history[sampler_set][sampler_tag].last_quanta % OPTGEN_VECTOR_SIZE;
            //and for prefetch hits, we train the last prefetch trigger PC
            if( !wrap && perset_optgen[set].should_cache(curr_quanta, last_quanta))
            {
                if(addr_history[sampler_set][sampler_tag].prefetched)
                    prefetch_predictor->increment(addr_history[sampler_set][sampler_tag].PC);
                else
                    demand_predictor->increment(addr_history[sampler_set][sampler_tag].PC);
            }
            else
            {
                //Train the predictor negatively because OPT would not have cached this line
                if(addr_history[sampler_set][sampler_tag].prefetched)
                    prefetch_predictor->decrement(addr_history[sampler_set][sampler_tag].PC);
                else
                    demand_predictor->decrement(addr_history[sampler_set][sampler_tag].PC);
            }
            //Some maintenance operations for OPTgen
            perset_optgen[set].add_access(curr_quanta);
            update_addr_history_lru(sampler_set, addr_history[sampler_set][sampler_tag].lru);

            //Since this was a demand access, mark the prefetched bit as false
            addr_history[sampler_set][sampler_tag].prefetched = false;
        }
        // This is the first time we are seeing this line (could be demand or prefetch)
        else if(addr_history[sampler_set].find(sampler_tag) == addr_history[sampler_set].end())
        {
            // Find a victim from the sampled cache if we are sampling
            if(addr_history[sampler_set].size() == SAMPLER_WAYS) 
                replace_addr_history_element(sampler_set);

            assert(addr_history[sampler_set].size() < SAMPLER_WAYS);
            //Initialize a new entry in the sampler
            addr_history[sampler_set][sampler_tag].init(curr_quanta);
            //If it's a prefetch, mark the prefetched bit;
            if(type == access_type::PREFETCH)
            {
                addr_history[sampler_set][sampler_tag].mark_prefetch();
                perset_optgen[set].add_prefetch(curr_quanta);
            }
            else
                perset_optgen[set].add_access(curr_quanta);
            update_addr_history_lru(sampler_set, SAMPLER_WAYS-1);
        }
        else //This line is a prefetch
        {
            assert(addr_history[sampler_set].find(sampler_tag) != addr_history[sampler_set].end());
            //if(hit && prefetched[set][way])
            uint64_t last_quanta = addr_history[sampler_set][sampler_tag].last_quanta % OPTGEN_VECTOR_SIZE;
            if (perset_mytimer[set] - addr_history[sampler_set][sampler_tag].last_quanta < 5*NUM_CORE) 
            {
                if(perset_optgen[set].should_cache(curr_quanta, last_quanta))
                {
                    if(addr_history[sampler_set][sampler_tag].prefetched)
                        prefetch_predictor->increment(addr_history[sampler_set][sampler_tag].PC);
                    else
                       demand_predictor->increment(addr_history[sampler_set][sampler_tag].PC);
                }
            }

            //Mark the prefetched bit
            addr_history[sampler_set][sampler_tag].mark_prefetch(); 
            //Some maintenance operations for OPTgen
            perset_optgen[set].add_prefetch(curr_quanta);
            update_addr_history_lru(sampler_set, addr_history[sampler_set][sampler_tag].lru);
        }

        // Get Hawkeye's prediction for this line
        bool new_prediction = demand_predictor->get_prediction (PC);
        if (type == access_type::PREFETCH)
            new_prediction = prefetch_predictor->get_prediction (PC);
        // Update the sampler with the timestamp, PC and our prediction
        // For prefetches, the PC will represent the trigger PC
        addr_history[sampler_set][sampler_tag].update(perset_mytimer[set], PC, new_prediction);
        addr_history[sampler_set][sampler_tag].lru = 0;
        //Increment the set timer
        perset_mytimer[set] = (perset_mytimer[set]+1) % TIMER_SIZE;
    }

    bool new_prediction = demand_predictor->get_prediction (PC);
    if (type == access_type::PREFETCH)
        new_prediction = prefetch_predictor->get_prediction (PC);

    signatures[static_cast<std::size_t>(set*NUM_WAY+way)] = PC;

    //Set RRIP values and age cache-friendly line
    if(!new_prediction)
        rrpv[static_cast<std::size_t>(set*NUM_WAY+way)] = maxRRPV;
    else
    {
        rrpv[static_cast<std::size_t>(set*NUM_WAY+way)] = 0;
        if(!hit)
        {
            bool saturated = false;
            for(long i=0; i<NUM_WAY; i++)
                if (rrpv[static_cast<std::size_t>(set*NUM_WAY+i)] == maxRRPV-1)
                    saturated = true;

            //Age all the cache-friendly  lines
            for(long i=0; i<NUM_WAY; i++)
            {
                if (!saturated && rrpv[static_cast<std::size_t>(set*NUM_WAY+i)] < maxRRPV-1)
                    rrpv[static_cast<std::size_t>(set*NUM_WAY+i)]++;
            }
        }
        rrpv[static_cast<std::size_t>(set*NUM_WAY+way)] = 0;
    }
}

// // use this function to print out your own stats on every heartbeat 
// void PrintStats_Heartbeat()
// {

// }

// use this function to print out your own stats at the end of simulation
void hawkeye::replacement_final_stats()
{
    uint64_t hits = 0;
    uint64_t accesses = 0;
    for(long i=0; i<NUM_SET; i++)
    {
        accesses += perset_optgen[static_cast<std::size_t>(i)].access;
        hits += perset_optgen[static_cast<std::size_t>(i)].get_num_opt_hits();
    }

    std::cout << "OPTgen accesses: " << accesses << std::endl;
    std::cout << "OPTgen hits: " << hits << std::endl;
    std::cout << "OPTgen hit rate: " << 100*(double)hits/(double)accesses << std::endl;

    std::cout << std::endl << std::endl;
    return;
}
