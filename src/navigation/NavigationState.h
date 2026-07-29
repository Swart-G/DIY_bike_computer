#pragma once

#include <Arduino.h>

namespace navigation {

enum class Lifecycle : uint8_t {
  Inactive = 0,
  Starting = 1,
  Navigating = 2,
  Rerouting = 3,
  Arrived = 4,
  Error = 5,
};

enum class Maneuver : uint8_t {
  Straight = 0,
  TurnLeft = 1,
  TurnRight = 2,
  SlightLeft = 3,
  SlightRight = 4,
  SharpLeft = 5,
  SharpRight = 6,
  Uturn = 7,
  Roundabout = 8,
  RoundaboutExit = 9,
  Destination = 10,
  Unknown = 255,
};

struct NavigationState {
  bool available = false;
  Lifecycle lifecycle = Lifecycle::Inactive;
  Maneuver maneuver = Maneuver::Unknown;
  Maneuver nextManeuver = Maneuver::Unknown;
  uint32_t distanceToManeuverM = 0;
  uint32_t nextDistanceM = 0;
  uint32_t remainingDistanceM = 0;
  int64_t etaUtcMs = 0;
  char street[65] = {0};
};

}  // namespace navigation
