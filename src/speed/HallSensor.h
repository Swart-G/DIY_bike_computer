#pragma once

#include <Arduino.h>

#include "config/app_config.h"

struct HallSensorSnapshot {
  uint32_t pulseCount = 0;
  uint32_t rejectedPulseCount = 0;
  uint32_t lastPulseMs = 0;
  uint32_t lastIntervalMs = 0;
  int pinLevel = HIGH;
};

class HallSensor {
 public:
  bool begin(const app::AppSettings& settings);
  void updateSettings(const app::AppSettings& settings);
  HallSensorSnapshot snapshot() const;

 private:
  static void IRAM_ATTR isrThunk();
  void IRAM_ATTR handleInterrupt();

  static HallSensor* instance_;
  volatile uint32_t pulseCount_ = 0;
  volatile uint32_t rejectedPulseCount_ = 0;
  volatile uint32_t lastPulseUs_ = 0;
  volatile uint32_t lastIntervalUs_ = 0;
  volatile uint32_t minPulseIntervalUs_ = 50000;

  bool started_ = false;
  bool pullupEnabled_ = true;
  int interruptMode_ = FALLING;
};
