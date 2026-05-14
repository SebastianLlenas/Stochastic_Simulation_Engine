#include "engine/damage.h"

#include <algorithm>
#include <limits>

#include "engine/utils.h"

namespace omega9 {
namespace {

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

uint32_t apply_fraction(uint32_t value, uint32_t num, uint32_t den) {
  if (den == 0) return 0;
  return static_cast<uint32_t>((static_cast<uint64_t>(value) * num) / den);
}

uint16_t apply_item_stat_boost(uint16_t stat, StatIndex index, ItemID item_id) {
  uint32_t num = 1u;
  uint32_t den = 1u;
  switch (item_id) {
    case ItemChoiceBand:
      if (index == StatAtk) {
        num = 3u;
        den = 2u;
      }
      break;
    case ItemChoiceSpecs:
      if (index == StatSpa) {
        num = 3u;
        den = 2u;
      }
      break;
    case ItemAssaultVest:
      if (index == StatSpd) {
        num = 3u;
        den = 2u;
      }
      break;
    case ItemEviolite:
      if (index == StatDef || index == StatSpd) {
        num = 3u;
        den = 2u;
      }
      break;
    default:
      break;
  }
  if (num == 1u && den == 1u) return stat;
  const uint32_t boosted = apply_fraction(stat, num, den);
  return static_cast<uint16_t>(std::min<uint32_t>(
      boosted, std::numeric_limits<uint16_t>::max()));
}

bool has_type(const ActiveMon &active, const BaseStats &base, TypeID type) {
  if (active.tera_state & TeraActive) {
    return active.tera_type == type;
  }
  if (type == base.type1) return true;
  return base.type2 < kTypeCount && type == base.type2;
}

bool is_base_type(TypeID type, const BaseStats &base) {
  if (type == base.type1) return true;
  return base.type2 < kTypeCount && type == base.type2;
}

void apply_type_multiplier(const StaticDatabase &db, TypeID move_type,
                           TypeID defender_type, uint32_t &num,
                           uint32_t &den, bool &zero_out,
                           bool ignore_immunity) {
  if (defender_type >= kTypeCount || zero_out) return;
  const float mult = db.type_multiplier(move_type, defender_type);
  if (mult == 0.0f) {
    if (!ignore_immunity) {
      zero_out = true;
    }
    return;
  }
  if (mult == 0.5f) {
    den *= 2u;
  } else if (mult == 2.0f) {
    num *= 2u;
  }
}

} // namespace

DamageResult calculate_damage(const BattleState &state, uint8_t attacker_side,
                              MoveID move_id, bool is_crit) {
  DamageResult result{};

  if (attacker_side >= kSides) return result;
  const uint8_t defender_side = attacker_side ^ 1u;

  const auto &db = StaticDatabase::instance();
  if (move_id == MoveNone || move_id >= db.move_count()) return result;

  const SideState &attacker_state = state.sides[attacker_side];
  const SideState &defender_state = state.sides[defender_side];
  const ActiveMon &attacker = attacker_state.active;
  const ActiveMon &defender = defender_state.active;

  if (attacker.species_id >= db.species_count()) return result;
  if (defender.species_id >= db.species_count()) return result;

  const BaseStats &attacker_base = db.base_stats(attacker.species_id);
  const BaseStats &defender_base = db.base_stats(defender.species_id);
  const MoveData &move = db.move_data(move_id);

  if (move.category == MoveStatus || move.power == 0) return result;

  StatIndex attack_stat = (move.category == MovePhysical) ? StatAtk : StatSpa;
  StatIndex defense_stat = (move.category == MovePhysical) ? StatDef : StatSpd;
  const ActiveMon *attack_mon = &attacker;
  const ActiveMon *defense_mon = &defender;
  const ActiveMon *stat_source = &attacker;
  const BaseStats *stat_base = &attacker_base;
  const BaseStats *defense_base = &defender_base;

  if (move_id == MovePsyshock) {
    attack_stat = StatSpa;
    defense_stat = StatDef;
  } else if (move_id == MoveBodyPress) {
    attack_stat = StatDef;
    defense_stat = StatDef;
  } else if (move_id == MoveFoulPlay) {
    attack_stat = StatAtk;
    defense_stat = StatDef;
    attack_mon = &defender;
    stat_source = &defender;
    stat_base = &defender_base;
  }

  uint16_t attack_value =
      get_real_stat(*stat_base, attack_stat, stat_source->level);
  uint16_t defense_value =
      get_real_stat(*defense_base, defense_stat, defense_mon->level);

  if (attack_value == 0 || defense_value == 0 || attacker.level == 0) {
    return result;
  }

  int attack_stage =
      clamp_stage(static_cast<int>(attack_mon->stat_stages[attack_stat]));
  int defense_stage =
      clamp_stage(static_cast<int>(defense_mon->stat_stages[defense_stat]));

  if (is_crit) {
    if (attack_stage < 0) attack_stage = 0;
    if (defense_stage > 0) defense_stage = 0;
  }

  attack_value = apply_stage(attack_value, attack_stage);
  defense_value = apply_stage(defense_value, defense_stage);
  if (defense_value == 0) return result;

  attack_value =
      apply_item_stat_boost(attack_value, attack_stat, attacker.item_id);
  defense_value =
      apply_item_stat_boost(defense_value, defense_stat, defense_mon->item_id);
  if (defense_value == 0) return result;

  if ((state.global_field_mask & FieldWeatherSand) &&
      defense_stat == StatSpd &&
      has_type(defender, defender_base, TypeRock)) {
    const uint32_t boosted = apply_fraction(defense_value, 3u, 2u);
    defense_value = static_cast<uint16_t>(std::min<uint32_t>(
        boosted, std::numeric_limits<uint16_t>::max()));
  }

  if ((state.global_field_mask & FieldWeatherSnow) &&
      defense_stat == StatDef &&
      has_type(defender, defender_base, TypeIce)) {
    const uint32_t boosted = apply_fraction(defense_value, 3u, 2u);
    defense_value = static_cast<uint16_t>(std::min<uint32_t>(
        boosted, std::numeric_limits<uint16_t>::max()));
  }

  const uint32_t level = attacker.level;
  uint32_t power = move.power;
  const bool tera_active = (attacker.tera_state & TeraActive) != 0;
  const bool multi_hit = move.min_hits > 1;
  if (tera_active && move.type == attacker.tera_type && move.priority <= 0 &&
      move.power > 0 && move.power < 60 && !multi_hit) {
    power = 60;
  }
  const bool attacker_grounded = is_grounded(attacker, attacker_base);
  const bool defender_grounded = is_grounded(defender, defender_base);

  if (state.global_field_mask & FieldTerrainElectric) {
    if (attacker_grounded && move.type == TypeElectric) {
      power = apply_fraction(power, 13u, 10u);
    }
  } else if (state.global_field_mask & FieldTerrainPsychic) {
    if (attacker_grounded && move.type == TypePsychic) {
      power = apply_fraction(power, 13u, 10u);
    }
  } else if (state.global_field_mask & FieldTerrainGrassy) {
    if (attacker_grounded && move.type == TypeGrass) {
      power = apply_fraction(power, 13u, 10u);
    }
    if (defender_grounded &&
        (move_id == MoveEarthquake || move_id == MoveBulldoze)) {
      power = apply_fraction(power, 1u, 2u);
    }
  } else if (state.global_field_mask & FieldTerrainMisty) {
    if (defender_grounded && move.type == TypeDragon) {
      power = apply_fraction(power, 1u, 2u);
    }
  }

  uint32_t type_num = 1u;
  uint32_t type_den = 1u;
  bool zero_out = false;
  if (move.type != TypeNone) {
    const bool ignore_immunity =
        (move.flags & MoveFlagIgnoreImmunity) != 0;
    const bool defender_tera = (defender.tera_state & TeraActive) != 0;
    if (defender_tera) {
      apply_type_multiplier(db, move.type, defender.tera_type, type_num,
                            type_den, zero_out, ignore_immunity);
    } else {
      apply_type_multiplier(db, move.type, defender_base.type1, type_num,
                            type_den, zero_out, ignore_immunity);
      apply_type_multiplier(db, move.type, defender_base.type2, type_num,
                            type_den, zero_out, ignore_immunity);
    }
  }

  if (move_id == MoveCollisionCourse && type_num > type_den && !zero_out) {
    power = apply_fraction(power, 5461u, 4096u);
  }

  uint32_t damage =
      static_cast<uint32_t>((2u * level) / 5u + 2u);
  uint64_t base = static_cast<uint64_t>(damage) * power * attack_value;
  base /= defense_value;
  base /= 50u;
  base += 2u;
  damage = static_cast<uint32_t>(base);

  if (state.global_field_mask & FieldWeatherSun) {
    if (move.type == TypeFire) {
      damage = apply_fraction(damage, 3u, 2u);
    } else if (move.type == TypeWater) {
      damage = apply_fraction(damage, 1u, 2u);
    }
  } else if (state.global_field_mask & FieldWeatherRain) {
    if (move.type == TypeWater) {
      damage = apply_fraction(damage, 3u, 2u);
    } else if (move.type == TypeFire) {
      damage = apply_fraction(damage, 1u, 2u);
    }
  }

  if (is_crit) {
    damage = apply_fraction(damage, 3u, 2u);
  }

  uint32_t stab_num = 1u;
  uint32_t stab_den = 1u;
  if (tera_active) {
    const bool tera_match = (move.type == attacker.tera_type);
    const bool original_match = is_base_type(move.type, attacker_base);
    if (tera_match) {
      if (is_base_type(attacker.tera_type, attacker_base)) {
        stab_num = 2u;
        stab_den = 1u;
      } else {
        stab_num = 3u;
        stab_den = 2u;
      }
    } else if (original_match) {
      stab_num = 3u;
      stab_den = 2u;
    }
  } else if (is_base_type(move.type, attacker_base)) {
    stab_num = 3u;
    stab_den = 2u;
  }

  uint32_t burn_num = 1u;
  uint32_t burn_den = 1u;
  if (move.category == MovePhysical && (attacker.major_status & StatusBurn)) {
    burn_num = 1u;
    burn_den = 2u;
  }

  uint32_t screen_num = 1u;
  uint32_t screen_den = 1u;
  if (!is_crit) {
    if (move.category == MovePhysical) {
      if (defender_state.side_condition_mask &
          (SideConditionReflect | SideConditionAuroraVeil)) {
        screen_den = 2u;
      }
    } else if (move.category == MoveSpecial) {
      if (defender_state.side_condition_mask &
          (SideConditionLightScreen | SideConditionAuroraVeil)) {
        screen_den = 2u;
      }
    }
  }

  uint32_t life_orb_num = 1u;
  uint32_t life_orb_den = 1u;
  if (attacker.item_id == ItemLifeOrb) {
    life_orb_num = 13u;
    life_orb_den = 10u;
  }

  if ((defender.volatile_mask & VolatileFlying) &&
      (move.flags & MoveFlagHitsFlying)) {
    damage *= 2u;
  }

  if ((defender.volatile_mask & VolatileDigging) &&
      (move.flags & MoveFlagHitsDigging)) {
    damage *= 2u;
  }

  for (std::size_t i = 0; i < result.rolls.size(); ++i) {
    uint32_t roll =
        apply_fraction(damage, static_cast<uint32_t>(85u + i), 100u);

    roll = apply_fraction(roll, stab_num, stab_den);

    if (zero_out) {
      roll = 0;
    } else {
      roll = apply_fraction(roll, type_num, type_den);
    }

    roll = apply_fraction(roll, burn_num, burn_den);
    roll = apply_fraction(roll, screen_num, screen_den);
    roll = apply_fraction(roll, life_orb_num, life_orb_den);

    if (roll == 0 && !zero_out) roll = 1u;

    result.rolls[i] = static_cast<uint16_t>(
        std::min<uint32_t>(roll, std::numeric_limits<uint16_t>::max()));
  }

  return result;
}

} // namespace omega9
