#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

#include "core/battle_state.h"
#include "data/database.h"

namespace omega9 {

inline int clamp_stage(int stage) {
  if (stage > 6) return 6;
  if (stage < -6) return -6;
  return stage;
}

inline uint16_t base_stat_value(const BaseStats &base, StatIndex index) {
  switch (index) {
    case StatAtk:
      return base.atk;
    case StatDef:
      return base.def;
    case StatSpa:
      return base.spa;
    case StatSpd:
      return base.spd;
    case StatSpe:
      return base.spe;
    default:
      return 0;
  }
}

inline uint16_t get_real_stat(const BaseStats &base, StatIndex index,
                              uint8_t level) {
  if (level == 0) return 0;
  constexpr uint8_t kPerfectIv = 31;
  constexpr uint8_t kRandbatsEv = 85;
  const uint32_t base_value = base_stat_value(base, index);
  const uint32_t ev = kRandbatsEv / 4u;
  uint32_t value = (2u * base_value + kPerfectIv + ev);
  value = (value * level) / 100u;
  value += 5u;
  return static_cast<uint16_t>(
      std::min<uint32_t>(value, std::numeric_limits<uint16_t>::max()));
}

inline bool is_grounded(const ActiveMon &active, const BaseStats &base) {
  if (active.volatile_mask & VolatileFlying) return false;

  if (active.tera_state & TeraActive) {
    if (active.tera_type == TypeFlying) return false;
  } else {
    if (base.type1 == TypeFlying || base.type2 == TypeFlying) return false;
  }

  return true;
}

} // namespace omega9
