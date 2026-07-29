#pragma once

#include <Arduino.h>

namespace media {

enum ActionMask : uint32_t {
  Play = 1UL << 0,
  Pause = 1UL << 1,
  Toggle = 1UL << 2,
  Next = 1UL << 3,
  Previous = 1UL << 4,
  Seek = 1UL << 5,
};

enum class Action : uint8_t {
  Play = 0,
  Pause = 1,
  Toggle = 2,
  Next = 3,
  Previous = 4,
  Seek = 5,
};

struct MediaState {
  bool available = false;
  bool playing = false;
  uint32_t supportedActions = 0;
  uint64_t durationMs = 0;
  uint64_t positionMs = 0;
  uint32_t receivedAtMs = 0;
  char player[33] = {0};
  char title[65] = {0};
  char artist[65] = {0};

  uint64_t positionNow(uint32_t nowMs) const {
    if (!playing || !durationMs) return min(positionMs, durationMs);
    const uint64_t estimated = positionMs + (nowMs - receivedAtMs);
    return min(estimated, durationMs);
  }
};

}  // namespace media
