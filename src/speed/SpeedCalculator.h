#pragma once

#include <Arduino.h>

#include "config/app_config.h"
#include "speed/HallSensor.h"

class SpeedCalculator {
 public:
  void reset();
  void update(const HallSensorSnapshot& sensor, const app::AppSettings& settings, uint32_t nowMs);

  float currentKmh() const { return currentKmh_; }
  uint32_t pulseCount() const { return lastPulseCount_; }
  uint32_t lastIntervalMs() const { return lastIntervalMs_; }

 private:
  float speedFromInterval(uint32_t intervalMs, float wheelCircumferenceM) const;

  float currentKmh_ = 0.0f;
  uint32_t lastPulseCount_ = 0;
  uint32_t lastPulseMs_ = 0;
  uint32_t lastIntervalMs_ = 0;
  bool hasValidInterval_ = false;
};
