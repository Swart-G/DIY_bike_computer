#pragma once

#include <Arduino.h>

#include "config/app_config.h"

enum class BatteryState { WarmingUp, Normal, Low, Critical, Charging, Stable, Discharging };

class BatteryMonitor {
 public:
  bool begin(const app::AppSettings& settings);
  void update(uint32_t nowMs);
  void updateSettings(const app::AppSettings& settings);
  bool enabled() const { return enabled_; }
  int adcPin() const { return adcPin_; }
  uint16_t rawAdc() const { return rawAdc_; }
  uint32_t adcMillivolts() const { return adcMillivolts_; }
  float instantVoltage() const { return instantVoltage_; }
  float voltage() const { return filteredVoltage_; }
  uint8_t percent() const { return percent_; }
  BatteryState state() const { return state_; }
  const char* trendText() const;
  String stateText() const;
  String statusText() const;
  String diagnosticText() const;

 private:
  static constexpr uint8_t kHistorySamples = 30;
  float estimatePercent(float voltage) const;
  void completeSeries(uint32_t nowMs);
  void pushTrend(float voltage);
  void updateState();

  bool enabled_ = false;
  int adcPin_ = -1;
  float calibrationFactor_ = 1.0f;
  uint8_t lowPercent_ = 29;
  uint8_t criticalPercent_ = 15;
  uint32_t lastSampleMs_ = 0;
  uint32_t seriesStartedMs_ = 0;
  uint8_t discardRemaining_ = 0;
  uint8_t seriesCount_ = 0;
  uint16_t rawAdc_ = 0;
  uint32_t adcMillivolts_ = 0;
  uint16_t seriesMv_[7] = {0};
  float instantVoltage_ = 0;
  float filteredVoltage_ = 0;
  uint8_t percent_ = 0;
  uint8_t stablePercent_ = 0;
  float trend_[kHistorySamples] = {0};
  uint8_t trendCount_ = 0;
  uint8_t trendIndex_ = 0;
  BatteryState state_ = BatteryState::WarmingUp;
};
