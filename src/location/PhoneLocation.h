#pragma once

#include <stdint.h>

namespace phonegeo {

enum FixFlags : uint8_t {
  HasAltitude = 1U << 0,
  HasAccuracy = 1U << 1,
  HasSpeed = 1U << 2,
};

static constexpr uint8_t kKnownFlags =
    FixFlags::HasAltitude | FixFlags::HasAccuracy | FixFlags::HasSpeed;
static constexpr uint32_t kFreshFixMs = 5000;

struct LocationFix {
  bool available = false;
  uint32_t rideId = 0;
  uint64_t timestampUtcMs = 0;
  int32_t latitudeE7 = 0;
  int32_t longitudeE7 = 0;
  int32_t altitudeMm = 0;
  uint32_t accuracyMm = 0;
  uint32_t speedMmps = 0;
  uint8_t flags = 0;
  uint32_t receivedAtMs = 0;

  bool validValues() const {
    if (!rideId || timestampUtcMs < 1577836800000ULL ||
        timestampUtcMs > 4102444800000ULL ||
        latitudeE7 < -900000000 || latitudeE7 > 900000000 ||
        longitudeE7 < -1800000000 || longitudeE7 > 1800000000 ||
        (flags & ~kKnownFlags) != 0) {
      return false;
    }
    if ((flags & FixFlags::HasAltitude) != 0 &&
        (altitudeMm < -2000000 || altitudeMm > 20000000)) {
      return false;
    }
    if ((flags & FixFlags::HasAccuracy) != 0 &&
        (!accuracyMm || accuracyMm > 1000000)) {
      return false;
    }
    if ((flags & FixFlags::HasSpeed) != 0 && speedMmps > 200000) {
      return false;
    }
    return true;
  }

  bool fresh(uint32_t nowMs) const {
    return available && static_cast<uint32_t>(nowMs - receivedAtMs) <= kFreshFixMs;
  }

  double latitude() const { return static_cast<double>(latitudeE7) / 10000000.0; }
  double longitude() const { return static_cast<double>(longitudeE7) / 10000000.0; }
  float altitudeM() const { return static_cast<float>(altitudeMm) / 1000.0f; }
  float accuracyM() const { return static_cast<float>(accuracyMm) / 1000.0f; }
  float speedMps() const { return static_cast<float>(speedMmps) / 1000.0f; }
};

struct LocationState {
  LocationFix fix;
  uint32_t acceptedCount = 0;
  uint32_t rejectedCount = 0;
};

}  // namespace phonegeo
