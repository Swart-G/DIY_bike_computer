#pragma once

#include <Arduino.h>

#include "battery/BatteryMonitor.h"
#include "config/app_config.h"
#include "speed/RideStateMachine.h"
#include "speed/SpeedCalculator.h"

class StorageManager;
class ClockManager;

class RideLogger {
 public:
  bool start(StorageManager& storage, const app::AppSettings& settings, const BatteryMonitor& battery, String& error);
  bool resume(StorageManager& storage, const RideRecoveryData& recovery, String& error);
  bool logSample(StorageManager& storage, const RideStateMachine& ride, const SpeedCalculator& speed,
                 const HallSensorSnapshot& sensor, const BatteryMonitor& battery, uint32_t nowMs);
  bool event(StorageManager& storage, const RideStateMachine& ride, const char* event, const char* details = "");
  bool finish(StorageManager& storage, const RideStateMachine& ride, const BatteryMonitor& battery, String& error);
  void close() { active_ = false; }
  bool active() const { return active_; }
  uint32_t rideId() const { return rideId_; }
  const char* folder() const { return folder_; }
  uint32_t sampleIndex() const { return sampleIndex_; }
  bool loggingGap() const { return loggingGap_; }
  float batteryStartVoltage() const { return batteryStart_; }
  float batteryMinVoltage() const { return batteryMin_; }
  float batteryMaxVoltage() const { return batteryMax_; }
  bool retryPending(StorageManager& storage, const RideStateMachine& ride);
  void setClock(ClockManager* clock) { clock_ = clock; }
  bool applyClockSync(StorageManager& storage, String& error);

 private:
  bool append(StorageManager& storage, const char* file, const char* line);
  bool writeMeta(StorageManager& storage, const app::AppSettings& settings, String& error);
  bool writeSummary(StorageManager& storage, const RideStateMachine& ride, const BatteryMonitor& battery, String& error);
  bool flushBuffered(StorageManager& storage);
  void queueSample(const char* line);
  uint32_t rideId_ = 0, sampleIndex_ = 0, lastSampleMs_ = 0, lastRetryMs_ = 0, sampleIntervalMs_ = 1000;
  char folder_[32] = {0};
  bool active_ = false, loggingGap_ = false;
  float batteryStart_ = 0, batteryMin_ = 0, batteryMax_ = 0;
  char buffered_[8][256] = {};
  uint8_t bufferedHead_ = 0, bufferedCount_ = 0;
  ClockManager* clock_ = nullptr;
  uint64_t rideStartedMonotonicMs_ = 0;
  int64_t startedAtUtcMs_ = 0;
};
