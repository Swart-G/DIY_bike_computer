#pragma once

#include <Arduino.h>

#include "config/app_config.h"
#include "led/SpeedTrendMath.h"

struct SpeedTrendReading {
  uint32_t windowMs = 0;
  float deltaKmh = 0.0f;
  SpeedTrendState state = SpeedTrendState::Stable;
  bool ready = false;
};

struct SpeedTrendSnapshot {
  SpeedTrendSnapshot() {
    readings[0].windowMs = 2000;
    readings[1].windowMs = 5000;
    readings[2].windowMs = 10000;
  }

  float currentKmh = 0.0f;
  SpeedTrendReading readings[3];
};

class SpeedTrendLed {
 public:
  void begin(const app::AppSettings& settings);
  void update(float speedKmh, const app::AppSettings& settings,
              uint32_t nowMs);
  SpeedTrendState state() const { return state_; }
  const SpeedTrendSnapshot& snapshot() const { return snapshot_; }
  void setDiagnosticRgb(uint8_t red, uint8_t green, uint8_t blue);
  void clearDiagnosticRgb();

 private:
  struct Sample {
    uint32_t timestampMs = 0;
    float speedKmh = 0.0f;
  };

  static constexpr uint32_t SAMPLE_INTERVAL_MS = 100;
  static constexpr uint8_t SAMPLE_CAPACITY = 104;

  void resetHistory();
  void appendSample(float speedKmh, uint32_t nowMs);
  bool baselineSample(uint32_t nowMs, uint32_t windowMs,
                      float& speedKmh) const;
  void show(SpeedTrendState state, uint8_t brightnessPercent);
  void off();

  Sample samples_[SAMPLE_CAPACITY];
  uint8_t writeIndex_ = 0;
  uint8_t sampleCount_ = 0;
  uint32_t lastSampleMs_ = 0;
  SpeedTrendState state_ = SpeedTrendState::Stable;
  SpeedTrendSnapshot snapshot_;
  bool ledOn_ = false;
  uint8_t shownBrightnessPercent_ = 0;
  bool diagnosticOverride_ = false;
};
