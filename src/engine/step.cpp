#include "engine/step.h"

#include <algorithm>
#include <cstdint>
#include <limits>

#include "data/database.h"
#include "engine/damage.h"
#include "engine/mutator.h"
#include "engine/switch.h"
#include "engine/utils.h"

namespace omega9 {
namespace {

constexpr float kMaxSeed = 0.999999f;

float clamp_seed(float seed) {
  if (seed < 0.0f) return 0.0f;
  if (seed > kMaxSeed) return kMaxSeed;
  return seed;
}

struct Rng {
  uint32_t state;
  explicit Rng(float seed) {
    const float clamped = clamp_seed(seed);
    state = static_cast<uint32_t>(clamped * 4294967295.0f);
    if (state == 0) {
      state = 0x6D2B79F5u;
    }
  }

  uint32_t next_u32() {
    state = state * 1664525u + 1013904223u;
    return state;
  }

  float next_float() {
    const uint32_t value = next_u32();
    return static_cast<float>(value >> 8) * (1.0f / 16777216.0f);
  }
};

constexpr uint8_t kPerfectIv = 31;
constexpr uint8_t kRandbatsEv = 85;

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

uint16_t fraction_amount(uint16_t max_hp, uint32_t num, uint32_t den) {
  if (max_hp == 0 || num == 0 || den == 0) return 0;
  const uint64_t raw =
      (static_cast<uint64_t>(max_hp) * static_cast<uint64_t>(num)) /
      static_cast<uint64_t>(den);
  return static_cast<uint16_t>(std::min<uint64_t>(
      raw, static_cast<uint64_t>(std::numeric_limits<uint16_t>::max())));
}

bool has_type(const ActiveMon &active, const BaseStats &base, TypeID type) {
  if (active.tera_state & TeraActive) {
    return active.tera_type == type;
  }
  if (base.type1 == type) return true;
  return base.type2 < kTypeCount && base.type2 == type;
}

bool check_immunity(const BattleState &state, uint8_t attacker_side,
                    MoveID move_id) {
  if (attacker_side >= kSides) return false;
  const uint8_t defender_side = attacker_side ^ 1u;
  const auto &db = StaticDatabase::instance();
  if (move_id == MoveNone || move_id >= db.move_count()) return false;

  const MoveData &move = db.move_data(move_id);
  if (move.type >= kTypeCount) return true;

  const ActiveMon &defender = state.sides[defender_side].active;
  if (defender.species_id == 0 || defender.species_id >= db.species_count()) {
    return true;
  }
  const BaseStats &base = db.base_stats(defender.species_id);

  if (move.flags & MoveFlagIgnoreImmunity) {
    return true;
  }

  auto is_immune = [&](TypeID defender_type) -> bool {
    return db.type_multiplier(move.type, defender_type) == 0.0f;
  };

  if (defender.tera_state & TeraActive) {
    return !is_immune(defender.tera_type);
  }

  if (base.type1 < kTypeCount && is_immune(base.type1)) return false;
  if (base.type2 < kTypeCount && is_immune(base.type2)) return false;

  return true;
}

bool is_choice_item(ItemID item_id) {
  return item_id == ItemChoiceBand || item_id == ItemChoiceSpecs ||
         item_id == ItemChoiceScarf;
}

bool has_magic_guard(const ActiveMon &) {
  return false;
}

void apply_fractional_damage(BattleState &state, uint8_t side, uint32_t num,
                             uint32_t den) {
  if (side >= kSides) return;
  ActiveMon &active = state.sides[side].active;
  if (active.hp == 0) return;
  const uint16_t max_hp = max_hp_for_active(active);
  uint16_t damage = fraction_amount(max_hp, num, den);
  if (damage == 0) damage = 1;
  StateMutator::apply_damage(state, side, damage, false, false);
}

void apply_fractional_heal(BattleState &state, uint8_t side, uint32_t num,
                           uint32_t den) {
  if (side >= kSides) return;
  ActiveMon &active = state.sides[side].active;
  if (active.hp == 0) return;
  const uint16_t max_hp = max_hp_for_active(active);
  uint16_t heal = fraction_amount(max_hp, num, den);
  if (heal == 0 && max_hp > 0 && num > 0) heal = 1;
  if (heal == 0) return;
  const uint32_t combined = static_cast<uint32_t>(active.hp) + heal;
  active.hp = static_cast<uint16_t>(
      std::min<uint32_t>(combined, static_cast<uint32_t>(max_hp)));
}

uint32_t apply_fraction(uint32_t value, uint32_t num, uint32_t den) {
  if (den == 0) return 0;
  return static_cast<uint32_t>((static_cast<uint64_t>(value) * num) / den);
}

uint16_t apply_stage(uint16_t stat, int stage) {
  if (stage > 6) stage = 6;
  if (stage < -6) stage = -6;
  if (stage >= 0) {
    const uint32_t num = static_cast<uint32_t>(2 + stage);
    return static_cast<uint16_t>((static_cast<uint32_t>(stat) * num) / 2u);
  }
  const uint32_t den = static_cast<uint32_t>(2 + (-stage));
  return static_cast<uint16_t>((static_cast<uint32_t>(stat) * 2u) / den);
}

uint16_t confusion_self_hit_damage(const ActiveMon &active,
                                   const BaseStats &base) {
  if (active.level == 0) return 0;
  uint16_t attack_value = get_real_stat(base, StatAtk, active.level);
  uint16_t defense_value = get_real_stat(base, StatDef, active.level);
  if (attack_value == 0 || defense_value == 0) return 0;

  const int attack_stage =
      clamp_stage(static_cast<int>(active.stat_stages[StatAtk]));
  const int defense_stage =
      clamp_stage(static_cast<int>(active.stat_stages[StatDef]));
  attack_value = apply_stage(attack_value, attack_stage);
  defense_value = apply_stage(defense_value, defense_stage);
  if (defense_value == 0) return 0;

  constexpr uint32_t kPower = 40u;
  uint32_t damage = (2u * active.level) / 5u + 2u;
  uint64_t base_damage =
      static_cast<uint64_t>(damage) * kPower * attack_value;
  base_damage /= defense_value;
  base_damage /= 50u;
  base_damage += 2u;
  const uint32_t capped = static_cast<uint32_t>(std::min<uint64_t>(
      base_damage, static_cast<uint64_t>(std::numeric_limits<uint16_t>::max())));
  return static_cast<uint16_t>(capped);
}

Action fallback_action(const BattleState &state, uint8_t side,
                       const Action &action) {
  Action normalized = action;
  normalized.side = side;

  if (normalized.is_switch()) {
    if (is_valid_action(state, normalized)) return normalized;
    for (BenchSlotIndex slot = 0; slot < kBenchSize; ++slot) {
      Action candidate = Action::make_switch(side, slot);
      if (is_valid_action(state, candidate)) return candidate;
    }
  }

  if (normalized.is_move()) {
    if (is_valid_action(state, normalized)) return normalized;
  }

  const ActiveMon &active = state.sides[side].active;
  for (std::size_t i = 0; i < active.moves.size(); ++i) {
    const MoveID move_id = active.moves[i];
    if (move_id == MoveNone) continue;
    if (active.pp[i] == 0) continue;
    Action candidate = Action::make_move(side, move_id, normalized.is_tera());
    if (is_valid_action(state, candidate)) return candidate;
    if (normalized.is_tera()) {
      candidate = Action::make_move(side, move_id, false);
      if (is_valid_action(state, candidate)) return candidate;
    }
  }
  return Action::make_move(side, MoveStruggle, false);
}

bool check_cant_move_free(BattleState &state, uint8_t side,
                          const MoveData &move, Rng &rng) {
  if (side >= kSides) return true;
  ActiveMon &active = state.sides[side].active;
  if (active.hp == 0) return true;

  if (active.major_status & StatusSleep) {
    if (active.sleep_turns > 0) {
      active.sleep_turns = static_cast<uint8_t>(active.sleep_turns - 1);
      if (active.sleep_turns == 0) {
        active.major_status =
            static_cast<uint8_t>(active.major_status & ~StatusSleep);
      } else {
        return true;
      }
    } else {
      active.major_status =
          static_cast<uint8_t>(active.major_status & ~StatusSleep);
    }
  }

  if (active.major_status & StatusFreeze) {
    if (move.flags & MoveFlagDefrost) {
      active.major_status =
          static_cast<uint8_t>(active.major_status & ~StatusFreeze);
      return false;
    }
    const float thaw_roll = rng.next_float();
    if (thaw_roll < 0.2f) {
      active.major_status =
          static_cast<uint8_t>(active.major_status & ~StatusFreeze);
    } else {
      return true;
    }
  }

  if (active.volatile_mask & VolatileFlinch) return true;

  return false;
}

bool check_cant_move_costly(BattleState &state, uint8_t side, Rng &rng) {
  if (side >= kSides) return true;
  ActiveMon &active = state.sides[side].active;
  if (active.hp == 0) return true;

  if (active.volatile_mask & VolatileConfused) {
    if (active.confusion_turns > 0) {
      active.confusion_turns =
          static_cast<uint8_t>(active.confusion_turns - 1);
    }
    if (active.confusion_turns == 0) {
      active.volatile_mask =
          static_cast<uint64_t>(active.volatile_mask & ~VolatileConfused);
    } else {
      if ((rng.next_u32() % 100u) < 33u) {
        const auto &db = StaticDatabase::instance();
        if (active.species_id < db.species_count()) {
          const BaseStats &base = db.base_stats(active.species_id);
          uint32_t damage = confusion_self_hit_damage(active, base);
          const uint32_t roll = 85u + (rng.next_u32() % 16u);
          damage = apply_fraction(damage, roll, 100u);
          StateMutator::apply_damage(state, side,
                                     static_cast<uint16_t>(damage), true,
                                     true);
        }
        return true;
      }
    }
  }

  if (active.major_status & StatusParalysis) {
    const float para_roll = rng.next_float();
    if (para_roll < 0.25f) {
      return true;
    }
  }

  return false;
}

uint16_t select_damage_roll(const DamageResult &result, float roll_sample) {
  const std::size_t size = result.rolls.size();
  std::size_t index =
      static_cast<std::size_t>(roll_sample * static_cast<float>(size));
  if (index >= size) index = size - 1;
  return result.rolls[index];
}

float calculate_crit_chance(const MoveData &move, const ActiveMon &attacker) {
  int stage = static_cast<int>(move.crit_ratio);
  if (attacker.volatile_mask & VolatileFocusEnergy) {
    stage += 2;
  }

  if (stage <= 0) return 1.0f / 24.0f;
  if (stage == 1) return 1.0f / 8.0f;
  if (stage == 2) return 0.5f;
  return 1.0f;
}

uint8_t resolve_hit_count(const MoveData &move, Rng &rng) {
  uint8_t min_hits = move.min_hits;
  uint8_t max_hits = move.max_hits;
  if (min_hits == 0) min_hits = 1;
  if (max_hits == 0) max_hits = min_hits;
  if (max_hits < min_hits) std::swap(max_hits, min_hits);
  if (min_hits == max_hits) return min_hits;

  if (min_hits == 2 && max_hits == 5) {
    const float roll = rng.next_float();
    if (roll < 0.35f) return 2;
    if (roll < 0.70f) return 3;
    if (roll < 0.85f) return 4;
    return 5;
  }

  const uint16_t range =
      static_cast<uint16_t>(max_hits - min_hits) + 1u;
  const uint16_t offset =
      static_cast<uint16_t>(rng.next_u32() % range);
  return static_cast<uint8_t>(min_hits + offset);
}

int find_move_slot(const ActiveMon &active, MoveID move_id) {
  for (std::size_t i = 0; i < active.moves.size(); ++i) {
    if (active.moves[i] == move_id) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool check_interruption(const BattleState &state, uint8_t attacker_side,
                        uint8_t defender_side, MoveID move_id) {
  static_cast<void>(attacker_side);
  if (defender_side >= kSides) return false;
  const auto &db = StaticDatabase::instance();
  if (move_id == MoveNone || move_id >= db.move_count()) return false;

  const ActiveMon &defender = state.sides[defender_side].active;
  const MoveData &move = db.move_data(move_id);

  if (defender.volatile_mask & VolatileSubstitute) {
    if (move.category == MoveStatus &&
        !(move.flags & (MoveFlagSound | MoveFlagBypassSubstitute))) {
      return true;
    }
  }

  if (defender.volatile_mask & VolatileProtect) {
    if (!(move.flags & MoveFlagBypassProtect)) {
      return true;
    }
  }

  if (defender.volatile_mask & VolatileFlying) {
    if (!(move.flags & MoveFlagHitsFlying)) {
      return true;
    }
  }

  if (defender.volatile_mask & VolatileDigging) {
    if (!(move.flags & MoveFlagHitsDigging)) {
      return true;
    }
  }

  return false;
}

bool execute_move(BattleState &state, const Action &action, Rng &rng) {
  const uint8_t attacker_side = action.side;
  if (attacker_side >= kSides) return false;
  SideState &attacker_state = state.sides[attacker_side];
  ActiveMon &attacker = attacker_state.active;
  const MoveID move_id = action.move_id();
  const bool is_protect = (move_id == MoveProtect);
  if (!is_protect) {
    attacker.protect_counter = 0;
  }

  if (action.is_tera() &&
      !(attacker_state.side_condition_mask & SideConditionTeraUsed)) {
    attacker_state.side_condition_mask |= SideConditionTeraUsed;
    attacker.tera_state =
        static_cast<uint8_t>(attacker.tera_state | TeraActive);
  }

  const uint8_t defender_side = attacker_side ^ 1u;

  if (state.sides[defender_side].active.hp == 0) {
    return true;
  }

  const auto &db = StaticDatabase::instance();
  if (move_id == MoveNone || move_id >= db.move_count()) return false;
  const MoveData &move = db.move_data(move_id);
  const bool is_struggle = (move_id == MoveStruggle);

  int move_slot = -1;
  if (!is_struggle) {
    move_slot = find_move_slot(attacker, move_id);
    if (move_slot < 0) return false;
    if (attacker.pp[move_slot] == 0) return false;
  }

  if (check_cant_move_free(state, attacker_side, move, rng)) return false;

  if (check_cant_move_costly(state, attacker_side, rng)) return false;

  if (!is_struggle) {
    attacker.pp[move_slot] = static_cast<uint8_t>(attacker.pp[move_slot] - 1);
  }

  if (is_choice_item(attacker.item_id) && move_id != MoveStruggle) {
    attacker.choice_move_id = move_id;
  }

  if (is_protect) {
    float protect_chance = 1.0f;
    for (uint8_t i = 0; i < attacker.protect_counter; ++i) {
      protect_chance /= 3.0f;
    }
    const float roll = rng.next_float();
    if (roll < protect_chance) {
      StateMutator::apply_volatile(state, attacker_side, VolatileProtect,
                                   rng.next_float());
      if (attacker.protect_counter <
          std::numeric_limits<uint8_t>::max()) {
        attacker.protect_counter =
            static_cast<uint8_t>(attacker.protect_counter + 1);
      }
    } else {
      attacker.protect_counter = 0;
    }
    return false;
  }

  if ((state.global_field_mask & FieldTerrainPsychic) &&
      move.priority > 0) {
    ActiveMon &defender = state.sides[defender_side].active;
    if (defender.species_id != 0 && defender.species_id < db.species_count()) {
      const BaseStats &defender_base = db.base_stats(defender.species_id);
      if (is_grounded(defender, defender_base) &&
          !(move.flags & MoveFlagTargetSelf)) {
        return false;
      }
    }
  }

  if (!(move.flags & MoveFlagTargetSelf)) {
    if (!check_immunity(state, attacker_side, move_id)) {
      return false;
    }

    if (check_interruption(state, attacker_side, defender_side, move_id)) {
      return false; // Move blocked by Protect or invulnerability.
    }
  }

  if (!(move.flags & MoveFlagTargetSelf)) {
    const uint8_t accuracy = move.accuracy;
    if (accuracy > 0) {
      uint32_t threshold = accuracy;
      int net_stage =
          static_cast<int>(attacker.stat_stages[StatAcc]) -
          static_cast<int>(
              state.sides[defender_side].active.stat_stages[StatEva]);
      if (net_stage > 6) net_stage = 6;
      if (net_stage < -6) net_stage = -6;

      uint32_t acc_num = 3u;
      uint32_t acc_den = 3u;
      if (net_stage >= 0) {
        acc_num = static_cast<uint32_t>(3 + net_stage);
      } else {
        acc_den = static_cast<uint32_t>(3 + (-net_stage));
      }
      threshold = apply_fraction(threshold, acc_num, acc_den);

      const uint32_t roll = rng.next_u32() % 100u;
      if (roll >= threshold) {
        return false;
      }
    }
  }

  ActiveMon &defender = state.sides[defender_side].active;
  const float crit_chance = calculate_crit_chance(move, attacker);
  const uint8_t hit_count = resolve_hit_count(move, rng);
  const bool hits_sub =
      (defender.volatile_mask & VolatileSubstitute) &&
      !(move.flags & (MoveFlagSound | MoveFlagBypassSubstitute));
  const bool bypass_substitute =
      (move.flags & (MoveFlagSound | MoveFlagBypassSubstitute)) != 0;
  bool any_hit_face = false;
  bool dealt_damage = false;
  for (uint8_t hit = 0; hit < hit_count; ++hit) {
    if (state.sides[defender_side].active.hp == 0) break;
    const bool current_hit_blocked =
        (defender.volatile_mask & VolatileSubstitute) && !bypass_substitute;
    if (!current_hit_blocked) {
      any_hit_face = true;
    }
    const bool is_crit = crit_chance >= 1.0f || rng.next_float() < crit_chance;
    const DamageResult damage = calculate_damage(state, attacker_side, move_id,
                                                 is_crit);
    const uint16_t roll = select_damage_roll(damage, rng.next_float());
    if (roll > 0) {
      dealt_damage = true;
    }
    StateMutator::apply_damage(state, defender_side, roll, true,
                               !current_hit_blocked);
    if (state.sides[defender_side].active.hp == 0) break;
  }

  if (!hits_sub && move.category == MoveStatus && move.primary_status != 0) {
    StateMutator::apply_status(state, defender_side, move.primary_status,
                               rng.next_float());
  }

  if (is_struggle) {
    const uint16_t max_hp = max_hp_for_active(attacker);
    const uint16_t recoil =
        std::max<uint16_t>(1u, static_cast<uint16_t>(max_hp / 4u));
    StateMutator::apply_damage(state, attacker_side, recoil, false, false);
  }

  if (dealt_damage && attacker.item_id == ItemLifeOrb &&
      !has_magic_guard(attacker)) {
    const uint16_t max_hp = max_hp_for_active(attacker);
    const uint16_t recoil =
        std::max<uint16_t>(1u, static_cast<uint16_t>(max_hp / 10u));
    StateMutator::apply_damage(state, attacker_side, recoil, false, false);
  }

  if (defender.major_status & StatusFreeze) {
    const bool defrosts =
        (move.type == TypeFire && move.category != MoveStatus);
    if (any_hit_face && defrosts) {
      defender.major_status =
          static_cast<uint8_t>(defender.major_status & ~StatusFreeze);
    }
  }

  if ((move.category == MoveStatus || dealt_damage) && defender.hp > 0 &&
      any_hit_face) {
    StateMutator::apply_secondary_effects(state, attacker_side, move_id,
                                          rng.next_float());
  }

  return state.sides[defender_side].active.hp == 0;
}

bool execute_action(BattleState &state, const Action &action, Rng &rng) {
  if (action.is_switch()) {
    execute_switch(state, action.side, action.bench_index());
    apply_entry_hazards(state, action.side);
    return state.sides[action.side].active.hp == 0;
  }
  return execute_move(state, action, rng);
}

void apply_weather_damage(BattleState &state) {
  const bool sandstorm = (state.global_field_mask & FieldWeatherSand) != 0;

  if (!sandstorm) return;

  const auto &db = StaticDatabase::instance();

  for (uint8_t side = 0; side < kSides; ++side) {
    ActiveMon &active = state.sides[side].active;
    if (active.hp == 0) continue;
    if (active.species_id == 0 || active.species_id >= db.species_count()) {
      continue;
    }
    const BaseStats &base = db.base_stats(active.species_id);
    bool immune = false;
    immune = has_type(active, base, TypeRock) ||
             has_type(active, base, TypeGround) ||
             has_type(active, base, TypeSteel);
    if (!immune) {
      apply_fractional_damage(state, side, 1u, 16u);
    }
  }
}

void apply_status_damage(BattleState &state) {
  for (uint8_t side = 0; side < kSides; ++side) {
    ActiveMon &active = state.sides[side].active;
    if (active.hp == 0) continue;

    const uint8_t status = active.major_status;
    if (status & StatusBurn) {
      apply_fractional_damage(state, side, 1u, 16u);
    } else if (status & StatusPoison) {
      apply_fractional_damage(state, side, 1u, 8u);
    } else if (status & StatusToxic) {
      const uint16_t max_hp = max_hp_for_active(active);
      uint16_t base = static_cast<uint16_t>(max_hp / 16u);
      if (base < 1u) base = 1u;
      const uint8_t turns =
          std::min<uint8_t>(static_cast<uint8_t>(active.toxic_turns + 1), 15u);
      const uint16_t damage =
          static_cast<uint16_t>(base * static_cast<uint16_t>(turns));
      StateMutator::apply_damage(state, side, damage, false, false);
    }
  }
}

void apply_leftovers(BattleState &state) {
  const auto &db = StaticDatabase::instance();
  const bool grassy =
      (state.global_field_mask & FieldTerrainGrassy) != 0;
  for (uint8_t side = 0; side < kSides; ++side) {
    ActiveMon &active = state.sides[side].active;
    if (active.hp == 0) continue;
    if (active.item_id == ItemLeftovers) {
      apply_fractional_heal(state, side, 1u, 16u);
    }
    if (grassy && active.species_id != 0 &&
        active.species_id < db.species_count()) {
      const BaseStats &base = db.base_stats(active.species_id);
      if (is_grounded(active, base)) {
        apply_fractional_heal(state, side, 1u, 16u);
      }
    }
  }
}

void decrement_counters(BattleState &state) {
  if (state.weather_turns > 0) {
    state.weather_turns = static_cast<uint8_t>(state.weather_turns - 1);
    if (state.weather_turns == 0) {
      const uint64_t weather_mask = FieldWeatherSun | FieldWeatherRain |
                                    FieldWeatherSand | FieldWeatherSnow;
      state.global_field_mask =
          static_cast<uint64_t>(state.global_field_mask & ~weather_mask);
    }
  }

  if (state.terrain_turns > 0) {
    state.terrain_turns = static_cast<uint8_t>(state.terrain_turns - 1);
    if (state.terrain_turns == 0) {
      const uint64_t terrain_mask = FieldTerrainElectric | FieldTerrainGrassy |
                                    FieldTerrainMisty | FieldTerrainPsychic;
      state.global_field_mask =
          static_cast<uint64_t>(state.global_field_mask & ~terrain_mask);
    }
  }

  if (state.trick_room_turns > 0) {
    state.trick_room_turns = static_cast<uint8_t>(state.trick_room_turns - 1);
    if (state.trick_room_turns == 0) {
      state.global_field_mask = static_cast<uint64_t>(
          state.global_field_mask & ~static_cast<uint64_t>(FieldRoomTrick));
    }
  }

  for (uint8_t side = 0; side < kSides; ++side) {
    SideState &side_state = state.sides[side];
    if (side_state.tailwind_turns > 0) {
      side_state.tailwind_turns =
          static_cast<uint8_t>(side_state.tailwind_turns - 1);
      if (side_state.tailwind_turns == 0) {
        side_state.side_condition_mask = static_cast<uint16_t>(
            side_state.side_condition_mask &
            static_cast<uint16_t>(~SideConditionTailwind));
      }
    }
    if (side_state.reflect_turns > 0) {
      side_state.reflect_turns =
          static_cast<uint8_t>(side_state.reflect_turns - 1);
      if (side_state.reflect_turns == 0) {
        side_state.side_condition_mask = static_cast<uint16_t>(
            side_state.side_condition_mask &
            static_cast<uint16_t>(~SideConditionReflect));
      }
    }
    if (side_state.light_screen_turns > 0) {
      side_state.light_screen_turns =
          static_cast<uint8_t>(side_state.light_screen_turns - 1);
      if (side_state.light_screen_turns == 0) {
        side_state.side_condition_mask = static_cast<uint16_t>(
            side_state.side_condition_mask &
            static_cast<uint16_t>(~SideConditionLightScreen));
      }
    }
    if (side_state.aurora_veil_turns > 0) {
      side_state.aurora_veil_turns =
          static_cast<uint8_t>(side_state.aurora_veil_turns - 1);
      if (side_state.aurora_veil_turns == 0) {
        side_state.side_condition_mask = static_cast<uint16_t>(
            side_state.side_condition_mask &
            static_cast<uint16_t>(~SideConditionAuroraVeil));
      }
    }
    side_state.active.volatile_mask &=
        ~static_cast<uint64_t>(VolatileFlinch | VolatileProtect |
                               VolatileEndure);
  }
}

void update_status_counters(BattleState &state) {
  for (uint8_t side = 0; side < kSides; ++side) {
    ActiveMon &active = state.sides[side].active;
    if (active.major_status & StatusToxic) {
      if (active.toxic_turns < std::numeric_limits<uint8_t>::max()) {
        active.toxic_turns = static_cast<uint8_t>(active.toxic_turns + 1);
      }
    } else {
      active.toxic_turns = 0;
    }
  }
}

bool bench_has_alive(const SideState &side) {
  for (const BenchMon &bench : side.bench) {
    if (bench.species_id == 0) continue;
    if (bench.hp_percent > 0) return true;
  }
  return false;
}

bool side_has_alive(const SideState &side) {
  if (side.active.hp > 0) return true;
  return bench_has_alive(side);
}

} // namespace

std::pair<BattleState, float> step(const BattleState &state, Action p1_action,
                                   Action p2_action, float random_seed) {
  BattleState next_state = state;

  if (next_state.force_switch_mask != 0) {
    const uint8_t mask = next_state.force_switch_mask;
    if (mask & 1u) {
      Action normalized = p1_action;
      normalized.side = 0;
      Action switch_action = normalized;
      if (!normalized.is_switch() ||
          !is_valid_action(next_state, normalized)) {
        switch_action = get_switch_action(next_state, 0);
      }
      if (switch_action.is_switch()) {
        execute_switch(next_state, 0, switch_action.bench_index());
        apply_entry_hazards(next_state, 0);
      }
      next_state.force_switch_mask = static_cast<uint8_t>(
          next_state.force_switch_mask & static_cast<uint8_t>(~1u));
    }
    if (mask & 2u) {
      Action normalized = p2_action;
      normalized.side = 1;
      Action switch_action = normalized;
      if (!normalized.is_switch() ||
          !is_valid_action(next_state, normalized)) {
        switch_action = get_switch_action(next_state, 1);
      }
      if (switch_action.is_switch()) {
        execute_switch(next_state, 1, switch_action.bench_index());
        apply_entry_hazards(next_state, 1);
      }
      next_state.force_switch_mask = static_cast<uint8_t>(
          next_state.force_switch_mask & static_cast<uint8_t>(~2u));
    }
    next_state.force_switch_mask = 0;
    return {next_state, 0.0f};
  }

  p1_action = fallback_action(state, 0, p1_action);
  p2_action = fallback_action(state, 1, p2_action);

  Rng rng(random_seed);
  const bool p1_first =
      goes_first(next_state, p1_action, p2_action, rng.next_float());

  const Action first_action = p1_first ? p1_action : p2_action;
  const Action second_action = p1_first ? p2_action : p1_action;

  const bool target_fainted = execute_action(next_state, first_action, rng);
  if (!target_fainted) {
    execute_action(next_state, second_action, rng);
  }

  apply_weather_damage(next_state);
  apply_leftovers(next_state);
  apply_status_damage(next_state);
  update_status_counters(next_state);
  decrement_counters(next_state);

  next_state.force_switch_mask = 0;
  if (next_state.sides[0].active.hp == 0 &&
      bench_has_alive(next_state.sides[0])) {
    next_state.force_switch_mask |= 1u;
  }
  if (next_state.sides[1].active.hp == 0 &&
      bench_has_alive(next_state.sides[1])) {
    next_state.force_switch_mask |= 2u;
  }

  const bool p1_alive = side_has_alive(next_state.sides[0]);
  const bool p2_alive = side_has_alive(next_state.sides[1]);

  float reward = 0.0f;
  if (p1_alive && !p2_alive) {
    reward = 1.0f;
  } else if (!p1_alive && p2_alive) {
    reward = -1.0f;
  }

  return {next_state, reward};
}

} // namespace omega9
