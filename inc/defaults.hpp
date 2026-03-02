/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DEFAULTS_HPP
#define DEFAULTS_HPP

#include "../branch/hashed_perceptron/hashed_perceptron.h"
#include "../btb/basic_btb/basic_btb.h"
#include "../prefetcher/no/no.h"
#include "../replacement/lru/lru.h"
#include "cache_builder.h"
#include "core_builder.h"
#include "ptw_builder.h"

namespace champsim::defaults
{
const auto default_core =
    champsim::modules::ModuleBuilder{}
        .add_parameter("dib_set", 32)
        .add_parameter("dib_way", 8)
        .add_parameter("dib_window", 16)
        .add_parameter("ifetch_buffer_size", 64)
        .add_parameter("decode_buffer_size", 32)
        .add_parameter("dispatch_buffer_size", 32)
        .add_parameter("dib_hit_buffer_size", 32) // assumed
        .add_parameter("register_file_size", 128)
        .add_parameter("rob_size", 352)
        .add_parameter("lq_size", 128)
        .add_parameter("sq_size", 72)
        .add_parameter("fetch_width", champsim::bandwidth::maximum_type{6})
        .add_parameter("decode_width", champsim::bandwidth::maximum_type{6})
        .add_parameter("dispatch_width", champsim::bandwidth::maximum_type{6})
        .add_parameter("execute_width", champsim::bandwidth::maximum_type{4})
        .add_parameter("lq_width", champsim::bandwidth::maximum_type{2})
        .add_parameter("sq_width", champsim::bandwidth::maximum_type{2})
        .add_parameter("retire_width", champsim::bandwidth::maximum_type{5})
        .add_parameter("dib_inorder_width", champsim::bandwidth::maximum_type{5}) // assumed
        .add_parameter("mispredict_penalty", 1)
        .add_parameter("schedule_width", champsim::bandwidth::maximum_type{128})
        .add_parameter("decode_latency", 1)
        .add_parameter("dib_hit_latency", 1)
        .add_parameter("dispatch_latency", 1)
        .add_parameter("schedule_latency", 0)
        .add_parameter("execute_latency", 0)
        .add_parameter("l1i_bandwidth", champsim::bandwidth::maximum_type{1})
        .add_parameter("l1d_bandwidth", champsim::bandwidth::maximum_type{1})
        .add_parameter("branch_predictor", "hashed_perceptron")
        .add_parameter("btb", "basic_btb");

const auto default_l1i = champsim::modules::ModuleBuilder{}
                             .add_parameter("sets_factor", 64)
                             .add_parameter("ways", 8)
                             .add_parameter("pq_size", 32)
                             .add_parameter("offset_bits", champsim::data::bits{LOG2_BLOCK_SIZE})
                             .add_parameter("reset_prefetch_as_load", false)
                             .add_parameter("virtual_prefetch", true)
                             .add_parameter("wq_checks_full_addr", true)
                             .add_parameter("prefetch_activate", std::vector<access_type>{access_type::LOAD, access_type::PREFETCH})
                             .add_parameter("prefetcher", "no")
                             .add_parameter("replacement", "lru");

const auto default_l1d = champsim::modules::ModuleBuilder{}
                             .add_parameter("sets_factor", 64)
                             .add_parameter("ways", 12)
                             .add_parameter("pq_size", 8)
                             .add_parameter("offset_bits", champsim::data::bits{LOG2_BLOCK_SIZE})
                             .add_parameter("reset_prefetch_as_load", false)
                             .add_parameter("virtual_prefetch", false)
                             .add_parameter("wq_checks_full_addr", true)
                             .add_parameter("prefetch_activate", std::vector<access_type>{access_type::LOAD, access_type::PREFETCH})
                             .add_parameter("prefetcher", "no")
                             .add_parameter("replacement", "lru");

const auto default_l2c = champsim::modules::ModuleBuilder{}
                             .add_parameter("sets_factor", 512)
                             .add_parameter("ways", 8)
                             .add_parameter("pq_size", 16)
                             .add_parameter("offset_bits", champsim::data::bits{LOG2_BLOCK_SIZE})
                             .add_parameter("reset_prefetch_as_load", false)
                             .add_parameter("virtual_prefetch", false)
                             .add_parameter("wq_checks_full_addr", true)
                             .add_parameter("prefetch_activate", std::vector<access_type>{access_type::LOAD, access_type::PREFETCH})
                             .add_parameter("prefetcher", "no")
                             .add_parameter("replacement", "lru");

const auto default_itlb = champsim::modules::ModuleBuilder{}
                              .add_parameter("sets_factor", 16)
                              .add_parameter("ways", 4)
                              .add_parameter("pq_size", 0)
                              .add_parameter("offset_bits", champsim::data::bits{LOG2_PAGE_SIZE})
                              .add_parameter("reset_prefetch_as_load", false)
                              .add_parameter("virtual_prefetch", true)
                              .add_parameter("wq_checks_full_addr", true)
                              .add_parameter("prefetch_activate", std::vector<access_type>{access_type::LOAD, access_type::PREFETCH})
                              .add_parameter("prefetcher", "no")
                              .add_parameter("replacement", "lru");

const auto default_dtlb = champsim::modules::ModuleBuilder{}
                              .add_parameter("sets_factor", 16)
                              .add_parameter("ways", 4)
                              .add_parameter("pq_size", 0)
                              .add_parameter("mshr_size", 8)
                              .add_parameter("offset_bits", champsim::data::bits{LOG2_PAGE_SIZE})
                              .add_parameter("reset_prefetch_as_load", false)
                              .add_parameter("virtual_prefetch", false)
                              .add_parameter("wq_checks_full_addr", true)
                              .add_parameter("prefetch_activate", std::vector<access_type>{access_type::LOAD, access_type::PREFETCH})
                              .add_parameter("prefetcher", "no")
                              .add_parameter("replacement", "lru");

const auto default_stlb = champsim::modules::ModuleBuilder{}
                              .add_parameter("sets_factor", 64)
                              .add_parameter("ways", 12)
                              .add_parameter("pq_size", 0)
                              .add_parameter("offset_bits", champsim::data::bits{LOG2_PAGE_SIZE})
                              .add_parameter("reset_prefetch_as_load", false)
                              .add_parameter("virtual_prefetch", false)
                              .add_parameter("wq_checks_full_addr", true)
                              .add_parameter("prefetch_activate", std::vector<access_type>{access_type::LOAD, access_type::PREFETCH})
                              .add_parameter("prefetcher", "no")
                              .add_parameter("replacement", "lru");

const auto default_llc = champsim::modules::ModuleBuilder{}
                             .add_parameter("name", "LLC")
                             .add_parameter("sets_factor", 2048)
                             .add_parameter("ways", 16)
                             .add_parameter("pq_size", 32)
                             .add_parameter("offset_bits", champsim::data::bits{LOG2_BLOCK_SIZE})
                             .add_parameter("reset_prefetch_as_load", false)
                             .add_parameter("virtual_prefetch", false)
                             .add_parameter("wq_checks_full_addr", true)
                             .add_parameter("prefetch_activate", std::vector<access_type>{access_type::LOAD, access_type::PREFETCH})
                             .add_parameter("prefetcher", "no")
                             .add_parameter("replacement", "lru");

const auto default_ptw = champsim::modules::ModuleBuilder{}
                              .add_parameter("bandwidth_factor", 2)
                              .add_parameter("mshr_factor", 5)
                              .add_parameter("pscl_5", std::vector<int>{1, 2})
                              .add_parameter("pscl_4", std::vector<int>{1, 4})
                              .add_parameter("pscl_3", std::vector<int>{2, 4})
                              .add_parameter("pscl_2", std::vector<int>{4, 8});
} // namespace champsim::defaults

#endif
