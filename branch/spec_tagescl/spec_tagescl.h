#ifndef BRANCH_SPEC_TAGESCL_H
#define BRANCH_SPEC_TAGESCL_H

#include <cstdint>

#include "address.h"
#include "modules.h"
#include "tagescl/tagescl.hpp"

/*
 * TAGE-SC-L (64 KB) from github.com/useredsa/spec_tage_scl, ported to the
 * class-based ChampSim module API. The upstream ChampSim interface targets
 * the legacy O3_CPU::* hook style; the library core under tagescl/ is
 * unmodified.
 */
struct spec_tagescl : champsim::modules::branch_predictor {
  using Impl = tagescl::Tage_SC_L<tagescl::CONFIG_64KB>;

  Impl impl{1}; // max in-flight branches: predictions resolve immediately here
  uint64_t last_ip = 0;
  uint32_t id = 0;
  // predict_branch() is called for every instruction, but last_branch_result()
  // only for actual branches; a dangling prediction must be retired as a
  // non-branch before the next one is issued.
  bool predicted = false;

  using branch_predictor::branch_predictor;

  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};

#endif
