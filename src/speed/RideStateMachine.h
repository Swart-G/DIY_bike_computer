#pragma once

#include <Arduino.h>

#include "config/app_config.h"

enum class RideState { IDLE, RIDING, PAUSED, FINISHED };
enum class MotionState : uint8_t { MOVING = 0, AUTO_PAUSED = 1 };

struct RideStats {
  float distanceM = 0;
  float maxSpeedKmh = 0;
  float averageMovingSpeedKmh = 0;
  float averageRecordingSpeedKmh = 0;
  uint64_t elapsedMs = 0;
  uint64_t recordingMs = 0;
  uint64_t movingMs = 0;
  uint64_t pauseMs = 0;
  uint64_t stoppedMs = 0;
  uint32_t pulseCount = 0;
  uint32_t rejectedPulseCount = 0;
};

struct RideRecoveryData {
  bool valid = false;
  uint32_t rideId = 0;
  char rideFolder[32] = {0};
  uint32_t lastSavedSampleIndex = 0;
  bool loggingGap = false;
  float batteryStartVoltage = 0;
  float batteryMinVoltage = 0;
  float batteryMaxVoltage = 0;
  RideStats stats;
  RideState lastState = RideState::IDLE;
};

class RideStateMachine {
 public:
  void begin(const app::AppSettings* settings);
  void update(uint32_t nowMs, float filteredSpeedKmh, uint32_t absolutePulseCount,
              uint32_t rejectedPulseCount = 0);
  void start(uint32_t nowMs, uint32_t absolutePulseCount);
  void pause(uint32_t nowMs);
  void resume(uint32_t nowMs);
  void finish(uint32_t nowMs);
  void newRide(uint32_t nowMs, uint32_t absolutePulseCount);
  void restorePaused(const RideRecoveryData& recovery, uint32_t nowMs, uint32_t absolutePulseCount);
  RideState state() const { return state_; }
  MotionState motionState() const { return motionState_; }
  const RideStats& stats() const { return stats_; }
  String stateText() const;
  bool needsRecoverySave(uint32_t nowMs, uint32_t intervalMs) const;
  RideRecoveryData recoveryData() const;
  void setRecoveryIdentity(uint32_t rideId, const char* rideFolder, uint32_t sampleIndex, bool loggingGap);
  void setRecoveryBattery(float startVoltage, float minVoltage, float maxVoltage);
  void markRecoverySaved(uint32_t nowMs);

 private:
  void updateAverages();
  const app::AppSettings* settings_ = nullptr;
  RideState state_ = RideState::IDLE;
  RideStats stats_;
  uint32_t lastUpdateMs_ = 0;
  uint32_t lastRecoverySaveMs_ = 0;
  uint32_t lastAbsolutePulseCount_ = 0;
  bool moving_ = false;
  MotionState motionState_ = MotionState::MOVING;
  uint32_t belowThresholdSinceMs_ = 0;
  uint32_t rideId_ = 0;
  char rideFolder_[32] = {0};
  uint32_t lastSavedSampleIndex_ = 0;
  bool loggingGap_ = false;
  float batteryStartVoltage_ = 0;
  float batteryMinVoltage_ = 0;
  float batteryMaxVoltage_ = 0;
};
