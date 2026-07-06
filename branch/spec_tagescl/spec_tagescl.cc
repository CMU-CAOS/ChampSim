#include "spec_tagescl.h"

#include <cassert>

#include "instruction.h"

bool spec_tagescl::predict_branch(champsim::address ip)
{
  if (predicted) {
    // The previous prediction was for a non-branch instruction: retire it
    // without updating any history.
    tagescl::Branch_Type type;
    type.is_conditional = false;
    type.is_indirect = false;
    impl.commit_state_at_retire(id, last_ip, type, false, 0);
  }
  id = impl.get_new_branch_id();
  bool prediction = impl.get_prediction(id, ip.to<uint64_t>());
  last_ip = ip.to<uint64_t>();
  predicted = true;
  return prediction;
}

void spec_tagescl::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  assert(predicted);
  assert(last_ip == ip.to<uint64_t>());
  tagescl::Branch_Type type;
  type.is_conditional = (branch_type == BRANCH_CONDITIONAL) || (branch_type == BRANCH_OTHER);
  type.is_indirect = (branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL) || (branch_type == BRANCH_RETURN)
                     || (branch_type == BRANCH_OTHER);

  impl.update_speculative_state(id, last_ip, type, taken, branch_target.to<uint64_t>());
  if (type.is_conditional) {
    impl.commit_state(id, last_ip, type, taken);
  }
  impl.commit_state_at_retire(id, last_ip, type, taken, branch_target.to<uint64_t>());
  predicted = false;
}
