#pragma once

#include <vector>

#include "core/battle_state.h"
#include "engine/action.h"

namespace omega9 {

struct BatchResult {
  std::vector<BattleState> states;
  std::vector<float> rewards;
};

BatchResult step_batch(const std::vector<BattleState> &states,
                       const std::vector<Action> &p1_actions,
                       const std::vector<Action> &p2_actions,
                       const std::vector<float> &random_seeds);

} // namespace omega9
