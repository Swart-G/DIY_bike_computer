#pragma once

#include <stdint.h>

enum class SpeedTrendState : uint8_t {
  Stable,
  Accelerating,
  Decelerating,
};

namespace speedtrend {

inline SpeedTrendState classify(float currentKmh, float baselineKmh,
                                float toleranceKmh) {
  const float delta = currentKmh - baselineKmh;
  if (delta > toleranceKmh) return SpeedTrendState::Accelerating;
  if (delta < -toleranceKmh) return SpeedTrendState::Decelerating;
  return SpeedTrendState::Stable;
}

}  // namespace speedtrend
