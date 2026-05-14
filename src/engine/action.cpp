#include "engine/action.h"

#include <limits>

#include "engine/utils.h"

namespace omega9 {
namespace {

int find_move_slot(const ActiveMon &active, MoveID move_id) {
  for (std::size_t i = 0; i < active.moves.size(); ++i) {
    if (active.moves[i] == move_id) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool has_type(const ActiveMon &active, TypeID type) {
  if (active.tera_state & TeraActive) {
    return active.tera_type == type;
  }
  const auto &db = StaticDatabase::instance();
  if (active.species_id >= db.species_count()) return false;
  const BaseStats &base = db.base_stats(active.species_id);
  if (base.type1 == type) return true;
  return base.type2 < kTypeCount && base.type2 == type;
}

bool is_choice_item(ItemID item_id) {
  return item_id == ItemChoiceBand || item_id == ItemChoiceSpecs ||
         item_id == ItemChoiceScarf;
}

} // namespace

Action Action::make_move(uint8_t side_index, MoveID move_id, bool is_tera) {
  Action action{};
  action.id = static_cast<uint16_t>(move_id);
  action.flags = is_tera ? kFlagTera : 0;
  action.side = side_index;
  return action;
}

Action Action::make_switch(uint8_t side_index, BenchSlotIndex bench_index) {
  Action action{};
  action.id = static_cast<uint16_t>(bench_index);
  action.flags = kFlagSwitch;
  action.side = side_index;
  return action;
}

uint16_t calculate_speed(const BattleState &state, uint8_t side_index) {
  if (side_index >= kSides) return 0;

  const SideState &side = state.sides[side_index];
  const ActiveMon &active = side.active;

  const auto &db = StaticDatabase::instance();
  if (active.species_id >= db.species_count()) return 0;
  const BaseStats &base = db.base_stats(active.species_id);

  uint32_t speed = get_real_stat(base, StatSpe, active.level);
  const int stage = clamp_stage(static_cast<int>(active.stat_stages[StatSpe]));

  if (stage >= 0) {
    speed = (speed * static_cast<uint32_t>(2 + stage)) / 2u;
  } else {
    speed = (speed * 2u) / static_cast<uint32_t>(2 + (-stage));
  }

  if (active.item_id == ItemChoiceScarf) {
    speed = (speed * 3u) / 2u;
  }

  if (state.global_field_mask & FieldWeatherSun) {
    // TODO: Apply Protosynthesis Speed boost when ability/volatile data exists.
  }

  if (side.side_condition_mask & SideConditionTailwind) {
    speed *= 2u;
  }

  if (active.major_status & StatusParalysis) {
    speed /= 2u; // Gen 9: paralysis halves Speed.
  }

  if (speed > std::numeric_limits<uint16_t>::max()) {
    speed = std::numeric_limits<uint16_t>::max();
  }

  return static_cast<uint16_t>(speed);
}

static int action_priority(const Action &action) {
  if (action.is_switch()) return 6;
  const auto &db = StaticDatabase::instance();
  if (action.move_id() >= db.move_count()) return 0;
  return db.move_data(action.move_id()).priority;
}

bool goes_first(const BattleState &state, const Action &a1, const Action &a2,
                float random_sample) {
  const int p1 = action_priority(a1);
  const int p2 = action_priority(a2);

  if (p1 != p2) return p1 > p2;

  const uint16_t s1 = calculate_speed(state, a1.side);
  const uint16_t s2 = calculate_speed(state, a2.side);

  if (s1 != s2) {
    const bool trick_room = (state.global_field_mask & FieldRoomTrick) != 0;
    return trick_room ? (s1 < s2) : (s1 > s2);
  }

  return random_sample > 0.5f;
}

bool is_valid_action(const BattleState &state, const Action &action) {
  if (action.side >= kSides) return false;

  const SideState &side = state.sides[action.side];

  if (action.is_move()) {
    if (side.active.hp == 0) return false;
    const MoveID move_id = action.move_id();
    if (move_id == MoveNone) return false;

    const auto &db = StaticDatabase::instance();
    if (move_id >= db.move_count()) return false;
    if (side.active.species_id >= db.species_count()) return false;
    const ActiveMon &active = side.active;
    if (action.is_tera()) {
      if (side.side_condition_mask & SideConditionTeraUsed) return false;
      if (active.tera_state & TeraActive) return false;
    }
    if (move_id == MoveStruggle) {
      bool all_empty = true;
      for (std::size_t i = 0; i < active.moves.size(); ++i) {
        if (active.moves[i] == MoveNone) continue;
        if (active.pp[i] > 0) {
          all_empty = false;
          break;
        }
      }
      if (all_empty) return true;
      if (active.choice_move_id != 0) {
        const int choice_slot = find_move_slot(active, active.choice_move_id);
        if (choice_slot >= 0 && active.pp[choice_slot] == 0) {
          return true;
        }
      }
      return false;
    }
    if (active.choice_move_id != 0) {
      if (move_id != active.choice_move_id) return false;
      const int choice_slot = find_move_slot(active, active.choice_move_id);
      if (choice_slot >= 0 && active.pp[choice_slot] == 0) return false;
    }
    const MoveData &move = db.move_data(move_id);
    if (active.item_id == ItemAssaultVest && move.category == MoveStatus) {
      return false;
    }
    const int slot = find_move_slot(active, move_id);
    if (slot < 0) return false;
    return active.pp[slot] > 0;
  }

  if (action.is_switch()) {
    if (side.active.volatile_mask & VolatileTrapped) {
      if (!has_type(side.active, TypeGhost)) return false;
    }
    const BenchSlotIndex slot = action.bench_index();
    if (slot >= kBenchSize) return false;
    const BenchMon &target = side.bench[slot];
    if (target.species_id == 0) return false;
    if (target.hp_percent == 0) return false;
    return true;
  }

  return false;
}

} // namespace omega9
