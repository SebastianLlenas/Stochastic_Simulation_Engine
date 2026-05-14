#pragma once

#include <utility>

#include "core/battle_state.h"
#include "engine/action.h"

namespace omega9 {

std::pair<BattleState, float> step(const BattleState &state, Action p1_action,
                                   Action p2_action, float random_seed);

} // namespace omega9
