#pragma once

#include <cstdint>
#include <type_traits>

#include "core/battle_state.h"
#include "data/database.h"

namespace omega9 {

using BenchSlotIndex = uint8_t;

enum class ActionKind : uint8_t {
  Move = 0,
  Switch = 1,
};

struct Action {
  uint16_t id = 0;    // MoveID for moves, BenchSlotIndex for switches.
  uint8_t flags = 0;  // bit 0: switch, bit 1: tera
  uint8_t side = 0;   // 0 or 1

  static constexpr uint8_t kFlagSwitch = 1u << 0;
  static constexpr uint8_t kFlagTera = 1u << 1;

  static Action make_move(uint8_t side_index, MoveID move_id, bool is_tera);
  static Action make_switch(uint8_t side_index, BenchSlotIndex bench_index);

  ActionKind kind() const { return (flags & kFlagSwitch) ? ActionKind::Switch : ActionKind::Move; }
  bool is_move() const { return (flags & kFlagSwitch) == 0; }
  bool is_switch() const { return (flags & kFlagSwitch) != 0; }
  bool is_tera() const { return (flags & kFlagTera) != 0; }

  MoveID move_id() const { return static_cast<MoveID>(id); }
  BenchSlotIndex bench_index() const { return static_cast<BenchSlotIndex>(id); }
};

static_assert(sizeof(Action) <= 4, "Action must be compact (<= 4 bytes).");
static_assert(std::is_trivially_copyable_v<Action>,
              "Action must be trivially copyable.");

uint16_t calculate_speed(const BattleState &state, uint8_t side_index);
bool goes_first(const BattleState &state, const Action &a1, const Action &a2,
                float random_sample);
bool is_valid_action(const BattleState &state, const Action &action);

} // namespace omega9
