#pragma once

#include <cstdint>

#include "core/battle_state.h"
#include "data/database.h"

namespace omega9 {

class StateMutator {
public:
  static void apply_damage(BattleState &state, uint8_t side, uint16_t damage,
                           bool is_direct_move_damage = false,
                           bool bypass_substitute = false);
  static void apply_status(BattleState &state, uint8_t side, uint8_t status_id,
                           float random_seed);
  static void apply_boost(BattleState &state, uint8_t side, StatIndex stat,
                          int amount);
  static void apply_volatile(BattleState &state, uint8_t side,
                             VolatileBits volatile_id, float random_seed);
  static void apply_secondary_effects(BattleState &state, uint8_t attacker_side,
                                      MoveID move_id, float random_seed);
  static void set_weather(BattleState &state, GlobalFieldBits weather,
                          uint8_t turns);
  static void set_terrain(BattleState &state, GlobalFieldBits terrain,
                          uint8_t turns);
  static void set_side_condition(BattleState &state, uint8_t side,
                                 SideConditionBits condition, uint8_t turns);
};

} // namespace omega9
