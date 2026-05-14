#pragma once

#include <array>
#include <cstdint>

#include "core/battle_state.h"
#include "data/database.h"

namespace omega9 {

struct DamageResult {
  std::array<uint16_t, 16> rolls{};
};

DamageResult calculate_damage(const BattleState &state, uint8_t attacker_side,
                              MoveID move_id, bool is_crit);

} // namespace omega9
