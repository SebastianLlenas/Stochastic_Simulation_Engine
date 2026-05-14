#include "engine/mutator.h"

#include <algorithm>
#include <limits>

#include "engine/utils.h"

namespace omega9 {
namespace {

constexpr float kMaxSeed = 0.999999f;
constexpr uint8_t kPerfectIv = 31;
constexpr uint8_t kRandbatsEv = 85;

float clamp_seed(float seed) {
  if (seed < 0.0f) return 0.0f;
  if (seed > kMaxSeed) return kMaxSeed;
  return seed;
}

bool roll_chance(float seed, uint8_t percent) {
  if (percent == 0) return false;
  const float threshold = static_cast<float>(percent) * 0.01f;
  return clamp_seed(seed) < threshold;
}

uint8_t sleep_turns_from_seed(float seed) {
  const float clamped = clamp_seed(seed);
  const int bucket = static_cast<int>(clamped * 3.0f); // 0..2
  return static_cast<uint8_t>(2 + bucket);
}

uint8_t confusion_turns_from_seed(float seed) {
  const float clamped = clamp_seed(seed);
  const int bucket = static_cast<int>(clamped * 4.0f); // 0..3
  return static_cast<uint8_t>(2 + bucket);
}

uint16_t max_hp_value(const BaseStats &base, uint8_t level) {
  if (level == 0) return 0;
  const uint32_t ev = kRandbatsEv / 4u;
  uint32_t value = (2u * base.hp + kPerfectIv + ev);
  value = (value * level) / 100u;
  value += static_cast<uint32_t>(level) + 10u;
  return static_cast<uint16_t>(
      std::min<uint32_t>(value, std::numeric_limits<uint16_t>::max()));
}

uint16_t max_hp_for_active(const ActiveMon &active) {
  if (active.species_id == 0) return 0;
  const auto &db = StaticDatabase::instance();
  if (active.species_id >= db.species_count()) return 0;
  return max_hp_value(db.base_stats(active.species_id), active.level);
}

bool has_type(const ActiveMon &active, const BaseStats &base, TypeID type) {
  if (active.tera_state & TeraActive) {
    return active.tera_type == type;
  }
  return base.type1 == type || base.type2 == type;
}

bool is_status_immune(const ActiveMon &active, const BaseStats &base,
                      uint8_t status_id) {
  if ((status_id & StatusParalysis) &&
      has_type(active, base, TypeElectric)) {
    return true;
  }
  if ((status_id & StatusBurn) && has_type(active, base, TypeFire)) {
    return true;
  }
  if ((status_id & (StatusPoison | StatusToxic)) &&
      (has_type(active, base, TypePoison) ||
       has_type(active, base, TypeSteel))) {
    return true;
  }
  if ((status_id & StatusFreeze) && has_type(active, base, TypeIce)) {
    return true;
  }
  return false;
}

int clamp_stage(int stage) {
  if (stage > 6) return 6;
  if (stage < -6) return -6;
  return stage;
}

} // namespace

void StateMutator::apply_damage(BattleState &state, uint8_t side,
                                uint16_t damage, bool is_direct_move_damage,
                                bool bypass_substitute) {
  if (side >= kSides) return;
  ActiveMon &active = state.sides[side].active;
  if (active.hp == 0) return;
  if (is_direct_move_damage &&
      (active.volatile_mask & VolatileSubstitute) != 0 &&
      !bypass_substitute && active.substitute_hp > 0) {
    if (damage >= active.substitute_hp) {
      active.substitute_hp = 0;
      active.volatile_mask =
          static_cast<uint64_t>(active.volatile_mask & ~VolatileSubstitute);
    } else {
      int sub_hp = static_cast<int>(active.substitute_hp);
      sub_hp -= static_cast<int>(damage);
      active.substitute_hp =
          static_cast<uint16_t>(std::max(0, sub_hp));
    }
    return;
  }
  if (is_direct_move_damage) {
    if ((active.volatile_mask & VolatileEndure) && damage >= active.hp) {
      damage = static_cast<uint16_t>(active.hp - 1u);
    }
  }
  if (is_direct_move_damage && active.item_id == ItemFocusSash) {
    const uint16_t max_hp = max_hp_for_active(active);
    if (max_hp > 0 && active.hp == max_hp && damage >= active.hp) {
      damage = static_cast<uint16_t>(active.hp - 1u);
      active.item_id = ItemNone;
    }
  }
  if (damage >= active.hp) {
    active.hp = 0;
    active.volatile_mask = 0;
    active.stat_stages.fill(0);
    active.major_status = 0;
    active.sleep_turns = 0;
    active.toxic_turns = 0;
    active.confusion_turns = 0;
    active.encore_turns = 0;
    active.protect_counter = 0;
    active.choice_move_id = 0;
    active.substitute_hp = 0;
    active.reserved.fill(0);
    return;
  }
  active.hp = static_cast<uint16_t>(active.hp - damage);
}

void StateMutator::apply_status(BattleState &state, uint8_t side,
                                uint8_t status_id, float random_seed) {
  if (side >= kSides || status_id == 0) return;

  ActiveMon &active = state.sides[side].active;
  if (active.major_status != 0) return;

  const auto &db = StaticDatabase::instance();
  if (active.species_id >= db.species_count()) return;
  const BaseStats &base = db.base_stats(active.species_id);

  const bool grounded = is_grounded(active, base);
  if (grounded) {
    if (state.global_field_mask & FieldTerrainMisty) return;
    if ((state.global_field_mask & FieldTerrainElectric) &&
        (status_id & StatusSleep)) {
      return;
    }
  }

  if (is_status_immune(active, base, status_id)) return;
  if ((status_id & StatusFreeze) &&
      (state.global_field_mask & FieldWeatherSun)) {
    return;
  }

  active.major_status = status_id;
  active.toxic_turns = 0;

  if (status_id & StatusSleep) {
    active.sleep_turns = sleep_turns_from_seed(random_seed);
  } else {
    active.sleep_turns = 0;
  }
}

void StateMutator::apply_boost(BattleState &state, uint8_t side, StatIndex stat,
                               int amount) {
  if (side >= kSides) return;
  if (stat >= StatCount) return;
  ActiveMon &active = state.sides[side].active;
  const int stage =
      clamp_stage(static_cast<int>(active.stat_stages[stat]) + amount);
  active.stat_stages[stat] = static_cast<int8_t>(stage);
}

void StateMutator::apply_volatile(BattleState &state, uint8_t side,
                                  VolatileBits volatile_id,
                                  float random_seed) {
  if (side >= kSides) return;
  ActiveMon &active = state.sides[side].active;
  if (volatile_id == VolatileConfused) {
    if (active.volatile_mask & VolatileConfused) {
      return;
    }
    const auto &db = StaticDatabase::instance();
    if (active.species_id >= db.species_count()) return;
    const BaseStats &base = db.base_stats(active.species_id);
    if ((state.global_field_mask & FieldTerrainMisty) &&
        is_grounded(active, base)) {
      return;
    }
    active.confusion_turns = confusion_turns_from_seed(random_seed);
  }
  active.volatile_mask |= static_cast<uint64_t>(volatile_id);
}

void StateMutator::apply_secondary_effects(BattleState &state,
                                           uint8_t attacker_side,
                                           MoveID move_id,
                                           float random_seed) {
  if (attacker_side >= kSides) return;
  const uint8_t defender_side = attacker_side ^ 1u;

  switch (move_id) {
    case MoveMoonblast:
      if (roll_chance(random_seed, 30)) {
        apply_boost(state, defender_side, StatSpa, -1);
      }
      break;
    case MoveShadowBall:
      if (roll_chance(random_seed, 20)) {
        apply_boost(state, defender_side, StatSpd, -1);
      }
      break;
    case MoveThunderPunch:
      if (roll_chance(random_seed, 10)) {
        apply_status(state, defender_side, StatusParalysis, random_seed);
      }
      break;
    default:
      break;
  }
}

void StateMutator::set_weather(BattleState &state, GlobalFieldBits weather,
                               uint8_t turns) {
  const uint64_t weather_mask = FieldWeatherSun | FieldWeatherRain |
                                FieldWeatherSand | FieldWeatherSnow;
  state.global_field_mask =
      static_cast<uint64_t>(state.global_field_mask & ~weather_mask);
  state.global_field_mask |= static_cast<uint64_t>(weather);
  state.weather_turns = turns;
}

void StateMutator::set_terrain(BattleState &state, GlobalFieldBits terrain,
                               uint8_t turns) {
  const uint64_t terrain_mask = FieldTerrainElectric | FieldTerrainGrassy |
                                FieldTerrainMisty | FieldTerrainPsychic;
  state.global_field_mask =
      static_cast<uint64_t>(state.global_field_mask & ~terrain_mask);
  state.global_field_mask |= static_cast<uint64_t>(terrain);
  state.terrain_turns = turns;
}

void StateMutator::set_side_condition(BattleState &state, uint8_t side,
                                      SideConditionBits condition,
                                      uint8_t turns) {
  if (side >= kSides) return;
  SideState &side_state = state.sides[side];
  const uint16_t mask = static_cast<uint16_t>(condition);
  side_state.side_condition_mask =
      static_cast<uint16_t>(side_state.side_condition_mask | mask);

  if (mask & SideConditionTailwind) {
    side_state.tailwind_turns = turns;
  }
  if (mask & SideConditionReflect) {
    side_state.reflect_turns = turns;
  }
  if (mask & SideConditionLightScreen) {
    side_state.light_screen_turns = turns;
  }
  if (mask & SideConditionAuroraVeil) {
    side_state.aurora_veil_turns = turns;
  }
}

} // namespace omega9
