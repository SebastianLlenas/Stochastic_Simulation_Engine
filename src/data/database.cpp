#include "data/database.h"

#include <algorithm>

namespace omega9 {

StaticDatabase &StaticDatabase::instance() {
  static StaticDatabase db;
  return db;
}

StaticDatabase::StaticDatabase() {
  init_type_chart();
  load_dummy_data();
}

const BaseStats &StaticDatabase::base_stats(SpeciesID id) const {
  return base_stats_[id];
}

const MoveData &StaticDatabase::move_data(MoveID id) const {
  return move_data_[id];
}

const RandomBattleSet &StaticDatabase::random_battle_set(SpeciesID id) const {
  return random_sets_[id];
}

void StaticDatabase::fill_moveset(SpeciesID id, std::array<MoveID, 4> &moves,
                                  std::array<uint8_t, 4> &pp) const {
  moves.fill(MoveNone);
  pp.fill(0);

  if (id >= random_sets_.size()) return;
  const RandomBattleSet &set = random_sets_[id];
  const std::size_t count =
      std::min<std::size_t>(set.moves.size(), moves.size());

  for (std::size_t i = 0; i < count; ++i) {
    const MoveID move_id = set.moves[i];
    moves[i] = move_id;
    if (move_id < move_data_.size()) {
      pp[i] = move_data_[move_id].pp;
    }
  }
}

float StaticDatabase::type_multiplier(TypeID attacker, TypeID defender) const {
  const std::size_t index = static_cast<std::size_t>(attacker) * kTypeCount +
                            static_cast<std::size_t>(defender);
  return type_chart_[index];
}

void StaticDatabase::load_dummy_data() {
  base_stats_.assign(SpeciesCount, BaseStats{});
  move_data_.assign(MoveCount, MoveData{});
  random_sets_.assign(SpeciesCount, RandomBattleSet{});

  base_stats_[SpeciesFlutterMane] =
      BaseStats{55, 55, 55, 135, 135, 135, TypeGhost, TypeFairy};
  base_stats_[SpeciesKoraidon] =
      BaseStats{100, 135, 115, 85, 100, 135, TypeFighting, TypeDragon};
  base_stats_[SpeciesIronHands] =
      BaseStats{154, 140, 108, 50, 68, 50, TypeFighting, TypeElectric};

  move_data_[MoveMoonblast] =
      MoveData{95, TypeFairy, 100, 0, MoveSpecial, 0, 0, 0, 1, 1, 15};
  move_data_[MoveShadowBall] =
      MoveData{80, TypeGhost, 100, 0, MoveSpecial, 0, 0, 0, 1, 1, 15};
  move_data_[MoveCollisionCourse] =
      MoveData{100, TypeFighting, 100, 0, MovePhysical, MoveFlagContact, 0, 0,
               1, 1, 5};
  move_data_[MoveDragonClaw] =
      MoveData{80, TypeDragon, 100, 0, MovePhysical, MoveFlagContact, 0, 0, 1,
               1, 15};
  move_data_[MoveThunderPunch] =
      MoveData{75, TypeElectric, 100, 0, MovePhysical,
               static_cast<uint8_t>(MoveFlagContact | MoveFlagPunch), 0, 0, 1,
               1, 15};
  move_data_[MoveDrainPunch] =
      MoveData{75, TypeFighting, 100, 0, MovePhysical,
               static_cast<uint8_t>(MoveFlagContact | MoveFlagPunch), 0, 0, 1,
               1, 10};
  move_data_[MovePsyshock] =
      MoveData{80, TypePsychic, 100, 0, MoveSpecial, 0, 0, 0, 1, 1, 10};
  move_data_[MoveBodyPress] =
      MoveData{80, TypeFighting, 100, 0, MovePhysical, 0, 0, 0, 1, 1, 10};
  move_data_[MoveFoulPlay] =
      MoveData{95, TypeDark, 100, 0, MovePhysical, 0, 0, 0, 1, 1, 15};
  move_data_[MoveEarthquake] =
      MoveData{100, TypeGround, 100, 0, MovePhysical, 0, 0, 0, 1, 1, 10};
  move_data_[MoveBulldoze] =
      MoveData{60, TypeGround, 100, 0, MovePhysical, 0, 0, 0, 1, 1, 20};
  move_data_[MoveStruggle] =
      MoveData{50, TypeNone, 0, 0, MovePhysical, 0, 0, 0, 1, 1, 1};
  move_data_[MoveProtect] =
      MoveData{0, TypeNormal, 0, 4, MoveStatus, MoveFlagTargetSelf, 0, 0, 1, 1,
               10};

  random_sets_[SpeciesFlutterMane].moves = {MoveMoonblast, MoveShadowBall};
  random_sets_[SpeciesFlutterMane].items = {ItemBoosterEnergy, ItemChoiceScarf,
                                            ItemFocusSash};

  random_sets_[SpeciesKoraidon].moves = {MoveCollisionCourse, MoveDragonClaw};
  random_sets_[SpeciesKoraidon].items = {ItemChoiceScarf, ItemLeftovers};

  random_sets_[SpeciesIronHands].moves = {MoveDrainPunch, MoveThunderPunch};
  random_sets_[SpeciesIronHands].items = {ItemBoosterEnergy, ItemLeftovers};
}

void StaticDatabase::init_type_chart() {
  type_chart_.fill(1.0f);

  auto set_multiplier = [this](TypeID attacker, TypeID defender, float value) {
    const std::size_t index = static_cast<std::size_t>(attacker) * kTypeCount +
                              static_cast<std::size_t>(defender);
    type_chart_[index] = value;
  };

  // Normal
  set_multiplier(TypeNormal, TypeRock, 0.5f);
  set_multiplier(TypeNormal, TypeSteel, 0.5f);
  set_multiplier(TypeNormal, TypeGhost, 0.0f);

  // Fire
  set_multiplier(TypeFire, TypeGrass, 2.0f);
  set_multiplier(TypeFire, TypeIce, 2.0f);
  set_multiplier(TypeFire, TypeBug, 2.0f);
  set_multiplier(TypeFire, TypeSteel, 2.0f);
  set_multiplier(TypeFire, TypeFire, 0.5f);
  set_multiplier(TypeFire, TypeWater, 0.5f);
  set_multiplier(TypeFire, TypeRock, 0.5f);
  set_multiplier(TypeFire, TypeDragon, 0.5f);

  // Water
  set_multiplier(TypeWater, TypeFire, 2.0f);
  set_multiplier(TypeWater, TypeGround, 2.0f);
  set_multiplier(TypeWater, TypeRock, 2.0f);
  set_multiplier(TypeWater, TypeWater, 0.5f);
  set_multiplier(TypeWater, TypeGrass, 0.5f);
  set_multiplier(TypeWater, TypeDragon, 0.5f);

  // Electric
  set_multiplier(TypeElectric, TypeWater, 2.0f);
  set_multiplier(TypeElectric, TypeFlying, 2.0f);
  set_multiplier(TypeElectric, TypeElectric, 0.5f);
  set_multiplier(TypeElectric, TypeGrass, 0.5f);
  set_multiplier(TypeElectric, TypeDragon, 0.5f);
  set_multiplier(TypeElectric, TypeGround, 0.0f);

  // Grass
  set_multiplier(TypeGrass, TypeWater, 2.0f);
  set_multiplier(TypeGrass, TypeGround, 2.0f);
  set_multiplier(TypeGrass, TypeRock, 2.0f);
  set_multiplier(TypeGrass, TypeFire, 0.5f);
  set_multiplier(TypeGrass, TypeGrass, 0.5f);
  set_multiplier(TypeGrass, TypePoison, 0.5f);
  set_multiplier(TypeGrass, TypeFlying, 0.5f);
  set_multiplier(TypeGrass, TypeBug, 0.5f);
  set_multiplier(TypeGrass, TypeDragon, 0.5f);
  set_multiplier(TypeGrass, TypeSteel, 0.5f);

  // Ice
  set_multiplier(TypeIce, TypeGrass, 2.0f);
  set_multiplier(TypeIce, TypeGround, 2.0f);
  set_multiplier(TypeIce, TypeFlying, 2.0f);
  set_multiplier(TypeIce, TypeDragon, 2.0f);
  set_multiplier(TypeIce, TypeFire, 0.5f);
  set_multiplier(TypeIce, TypeWater, 0.5f);
  set_multiplier(TypeIce, TypeIce, 0.5f);
  set_multiplier(TypeIce, TypeSteel, 0.5f);

  // Fighting
  set_multiplier(TypeFighting, TypeNormal, 2.0f);
  set_multiplier(TypeFighting, TypeIce, 2.0f);
  set_multiplier(TypeFighting, TypeRock, 2.0f);
  set_multiplier(TypeFighting, TypeDark, 2.0f);
  set_multiplier(TypeFighting, TypeSteel, 2.0f);
  set_multiplier(TypeFighting, TypePoison, 0.5f);
  set_multiplier(TypeFighting, TypeFlying, 0.5f);
  set_multiplier(TypeFighting, TypePsychic, 0.5f);
  set_multiplier(TypeFighting, TypeBug, 0.5f);
  set_multiplier(TypeFighting, TypeFairy, 0.5f);
  set_multiplier(TypeFighting, TypeGhost, 0.0f);

  // Poison
  set_multiplier(TypePoison, TypeGrass, 2.0f);
  set_multiplier(TypePoison, TypeFairy, 2.0f);
  set_multiplier(TypePoison, TypePoison, 0.5f);
  set_multiplier(TypePoison, TypeGround, 0.5f);
  set_multiplier(TypePoison, TypeRock, 0.5f);
  set_multiplier(TypePoison, TypeGhost, 0.5f);
  set_multiplier(TypePoison, TypeSteel, 0.0f);

  // Ground
  set_multiplier(TypeGround, TypeFire, 2.0f);
  set_multiplier(TypeGround, TypeElectric, 2.0f);
  set_multiplier(TypeGround, TypePoison, 2.0f);
  set_multiplier(TypeGround, TypeRock, 2.0f);
  set_multiplier(TypeGround, TypeSteel, 2.0f);
  set_multiplier(TypeGround, TypeGrass, 0.5f);
  set_multiplier(TypeGround, TypeBug, 0.5f);
  set_multiplier(TypeGround, TypeFlying, 0.0f);

  // Flying
  set_multiplier(TypeFlying, TypeGrass, 2.0f);
  set_multiplier(TypeFlying, TypeFighting, 2.0f);
  set_multiplier(TypeFlying, TypeBug, 2.0f);
  set_multiplier(TypeFlying, TypeElectric, 0.5f);
  set_multiplier(TypeFlying, TypeRock, 0.5f);
  set_multiplier(TypeFlying, TypeSteel, 0.5f);

  // Psychic
  set_multiplier(TypePsychic, TypeFighting, 2.0f);
  set_multiplier(TypePsychic, TypePoison, 2.0f);
  set_multiplier(TypePsychic, TypePsychic, 0.5f);
  set_multiplier(TypePsychic, TypeSteel, 0.5f);
  set_multiplier(TypePsychic, TypeDark, 0.0f);

  // Bug
  set_multiplier(TypeBug, TypeGrass, 2.0f);
  set_multiplier(TypeBug, TypePsychic, 2.0f);
  set_multiplier(TypeBug, TypeDark, 2.0f);
  set_multiplier(TypeBug, TypeFire, 0.5f);
  set_multiplier(TypeBug, TypeFighting, 0.5f);
  set_multiplier(TypeBug, TypePoison, 0.5f);
  set_multiplier(TypeBug, TypeFlying, 0.5f);
  set_multiplier(TypeBug, TypeGhost, 0.5f);
  set_multiplier(TypeBug, TypeSteel, 0.5f);
  set_multiplier(TypeBug, TypeFairy, 0.5f);

  // Rock
  set_multiplier(TypeRock, TypeFire, 2.0f);
  set_multiplier(TypeRock, TypeIce, 2.0f);
  set_multiplier(TypeRock, TypeFlying, 2.0f);
  set_multiplier(TypeRock, TypeBug, 2.0f);
  set_multiplier(TypeRock, TypeFighting, 0.5f);
  set_multiplier(TypeRock, TypeGround, 0.5f);
  set_multiplier(TypeRock, TypeSteel, 0.5f);

  // Ghost
  set_multiplier(TypeGhost, TypePsychic, 2.0f);
  set_multiplier(TypeGhost, TypeGhost, 2.0f);
  set_multiplier(TypeGhost, TypeDark, 0.5f);
  set_multiplier(TypeGhost, TypeNormal, 0.0f);

  // Dragon
  set_multiplier(TypeDragon, TypeDragon, 2.0f);
  set_multiplier(TypeDragon, TypeSteel, 0.5f);
  set_multiplier(TypeDragon, TypeFairy, 0.0f);

  // Dark
  set_multiplier(TypeDark, TypePsychic, 2.0f);
  set_multiplier(TypeDark, TypeGhost, 2.0f);
  set_multiplier(TypeDark, TypeFighting, 0.5f);
  set_multiplier(TypeDark, TypeDark, 0.5f);
  set_multiplier(TypeDark, TypeFairy, 0.5f);

  // Steel
  set_multiplier(TypeSteel, TypeIce, 2.0f);
  set_multiplier(TypeSteel, TypeRock, 2.0f);
  set_multiplier(TypeSteel, TypeFairy, 2.0f);
  set_multiplier(TypeSteel, TypeFire, 0.5f);
  set_multiplier(TypeSteel, TypeWater, 0.5f);
  set_multiplier(TypeSteel, TypeElectric, 0.5f);
  set_multiplier(TypeSteel, TypeSteel, 0.5f);

  // Fairy
  set_multiplier(TypeFairy, TypeFighting, 2.0f);
  set_multiplier(TypeFairy, TypeDragon, 2.0f);
  set_multiplier(TypeFairy, TypeDark, 2.0f);
  set_multiplier(TypeFairy, TypeFire, 0.5f);
  set_multiplier(TypeFairy, TypePoison, 0.5f);
  set_multiplier(TypeFairy, TypeSteel, 0.5f);
}

} // namespace omega9
