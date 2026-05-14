#pragma once

#include <cstdint>

#include "core/battle_state.h"
#include "engine/action.h"

namespace omega9 {

void execute_switch(BattleState &state, uint8_t side, BenchSlotIndex slot);
void apply_entry_hazards(BattleState &state, uint8_t side);
Action get_switch_action(const BattleState &state, uint8_t side);

} // namespace omega9
