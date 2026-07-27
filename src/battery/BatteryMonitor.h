#pragma once

#include <Arduino.h>

enum class BatteryState {
  NotConfigured,
  WarmingUp,
  Charging,
  FullOrNearFull,
  Discharging,
  Stable,
};

class BatteryMonitor {
 public:
  bool begin();
  void update(uint32_t nowMs);
  bool enabled() const { return enabled_; }
  int adcPin() const { return adcPin_; }
  float voltage() const { return filteredVoltage_; }
  BatteryState state() const { return state_; }
  String stateText() const;
  String statusText() const;
  String diagnosticText() const;

 private:
  static constexpr uint8_t kHistorySamples = 120;
  static constexpr uint8_t kCompareWindowSamples = 40;

  float readBatteryVoltage() const;
  float averageWindow(uint8_t newestOffset, uint8_t count) const;
  void pushSample(float voltage);
  void updateState();

  bool enabled_ = false;
  int adcPin_ = -1;
  uint32_t lastSampleMs_ = 0;
  float filteredVoltage_ = 0.0f;
  float samples_[kHistorySamples] = {0.0f};
  uint8_t writeIndex_ = 0;
  uint8_t sampleCount_ = 0;
  BatteryState state_ = BatteryState::NotConfigured;
};
