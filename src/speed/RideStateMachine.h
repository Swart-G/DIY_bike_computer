#pragma once

#include <Arduino.h>

#include "config/app_config.h"

enum class RideState {
  IDLE,
  RIDING,
  PAUSED,
  FINISHED,
};

struct RideStats {
  float distanceM = 0.0f;
  float maxSpeedKmh = 0.0f;
  float avgSpeedKmh = 0.0f;
  uint32_t elapsedMs = 0;
  uint32_t movingMs = 0;
  uint32_t pauseMs = 0;
  uint32_t pulseCount = 0;
};

struct RideRecoveryData {
  bool valid = false;
  RideStats stats;
  RideState lastState = RideState::IDLE;
};

class RideStateMachine {
 public:
  void begin(const app::AppSettings* settings);
  void update(uint32_t nowMs, float currentSpeedKmh, uint32_t absolutePulseCount);

  void start(uint32_t nowMs, uint32_t absolutePulseCount);
  void pause(uint32_t nowMs);
  void resume(uint32_t nowMs);
  void finish(uint32_t nowMs);
  void newRide(uint32_t nowMs, uint32_t absolutePulseCount);
  void restorePaused(const RideRecoveryData& recovery, uint32_t nowMs, uint32_t absolutePulseCount);

  RideState state() const { return state_; }
  const RideStats& stats() const { return stats_; }
  String stateText() const;
  bool needsRecoverySave(uint32_t nowMs, uint32_t intervalMs) const;
  RideRecoveryData recoveryData() const;
  void markRecoverySaved(uint32_t nowMs);

 private:
  const app::AppSettings* settings_ = nullptr;
  RideState state_ = RideState::IDLE;
  RideStats stats_;

  uint32_t rideStartMs_ = 0;
  uint32_t stateEnteredMs_ = 0;
  uint32_t lastUpdateMs_ = 0;
  uint32_t lastRecoverySaveMs_ = 0;
  uint32_t lastAbsolutePulseCount_ = 0;
};
