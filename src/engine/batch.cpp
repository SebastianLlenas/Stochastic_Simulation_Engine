#include "engine/batch.h"

#include <cstddef>
#include <cstdint>

#include "engine/step.h"

namespace omega9 {

BatchResult step_batch(const std::vector<BattleState> &states,
                       const std::vector<Action> &p1_actions,
                       const std::vector<Action> &p2_actions,
                       const std::vector<float> &random_seeds) {
  const std::size_t count = states.size();

  BatchResult result;
  result.states.resize(count);
  result.rewards.resize(count);

  const BattleState *state_data = states.data();
  const Action *p1_data = p1_actions.data();
  const Action *p2_data = p2_actions.data();
  const float *seed_data = random_seeds.data();
  BattleState *out_states = result.states.data();
  float *out_rewards = result.rewards.data();

#pragma omp parallel for
  for (std::int64_t i = 0; i < static_cast<std::int64_t>(count); ++i) {
    const std::size_t index = static_cast<std::size_t>(i);
    const auto step_result =
        step(state_data[index], p1_data[index], p2_data[index],
             seed_data[index]);
    out_states[index] = step_result.first;
    out_rewards[index] = step_result.second;
  }

  return result;
}

} // namespace omega9
