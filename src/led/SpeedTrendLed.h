#pragma once

#include <Arduino.h>

#include "config/app_config.h"
#include "led/SpeedTrendMath.h"

class SpeedTrendLed {
 public:
  static constexpr uint32_t COMPARISON_WINDOW_MS = 2000;

  void begin(const app::AppSettings& settings);
  void update(float speedKmh, const app::AppSettings& settings,
              uint32_t nowMs);
  SpeedTrendState state() const { return state_; }

 private:
  struct Sample {
    uint32_t timestampMs = 0;
    float speedKmh = 0.0f;
  };

  static constexpr uint32_t SAMPLE_INTERVAL_MS = 100;
  static constexpr uint8_t SAMPLE_CAPACITY = 24;

  void resetHistory();
  void appendSample(float speedKmh, uint32_t nowMs);
  bool baselineSample(uint32_t nowMs, float& speedKmh) const;
  void show(SpeedTrendState state, uint8_t brightnessPercent);
  void off();

  Sample samples_[SAMPLE_CAPACITY];
  uint8_t writeIndex_ = 0;
  uint8_t sampleCount_ = 0;
  uint32_t lastSampleMs_ = 0;
  SpeedTrendState state_ = SpeedTrendState::Stable;
  bool ledOn_ = false;
  uint8_t shownBrightnessPercent_ = 0;
};
