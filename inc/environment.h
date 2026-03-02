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

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <functional>
#include <vector>

#include "modules.h"

namespace champsim
{
struct environment {
  virtual std::vector<std::reference_wrapper<champsim::modules::core_module>> cpu_view() = 0;
  virtual std::vector<std::reference_wrapper<champsim::modules::cache_module>> cache_view() = 0;
  virtual std::vector<std::reference_wrapper<champsim::modules::page_table_walker_module>> ptw_view() = 0;
  virtual champsim::modules::memory_controller_module& dram_view() = 0;
  virtual std::vector<std::reference_wrapper<operable>> operable_view() = 0;
};

namespace configured
{
template <unsigned long long ID>
struct generated_environment;

template <typename R>
auto build(champsim::modules::ModuleBuilder builder)
{
  return modules::module_base<R,champsim::environment>::create_instance(builder);
}

template <typename R, typename... ModuleBuilders>
auto build_many(ModuleBuilders... builders)
{
  std::vector<R*> retval{};
  retval.reserve(sizeof...(builders));
  (..., retval.push_back(modules::module_base<R,champsim::environment>::create_instance(builders)));
  return retval;
}
} // namespace configured
} // namespace champsim

#endif
