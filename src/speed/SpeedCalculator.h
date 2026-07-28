#pragma once
#include <Arduino.h>
#include "config/app_config.h"
#include "speed/HallSensor.h"
class SpeedCalculator {
 public:
  void reset();
  void update(const HallSensorSnapshot& sensor, const app::AppSettings& settings, uint32_t nowMs);
  float currentKmh() const { return displayKmh_; }
  float rawKmh() const { return rawKmh_; }
  float filteredKmh() const { return filteredKmh_; }
  uint32_t pulseCount() const { return lastPulseCount_; }
  uint64_t lastIntervalUs() const { return lastIntervalUs_; }
  uint64_t timeSincePulseUs() const;
 private:
  float speedFromInterval(uint64_t us, const app::AppSettings& settings) const;
  float rawKmh_ = 0, filteredKmh_ = 0, displayKmh_ = 0;
  uint32_t lastPulseCount_ = 0;
  uint64_t lastPulseUs_ = 0, lastIntervalUs_ = 0;
  bool hasValidInterval_ = false;
};
