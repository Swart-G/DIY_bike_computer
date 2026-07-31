#pragma once

#include <Arduino.h>

#include "config/app_config.h"

enum class BatteryState { WarmingUp, Normal, Low, Critical, Charging, Stable, Discharging };
enum class RuntimeEstimateQuality : uint8_t { Unavailable, Early, Learned };

class BatteryMonitor {
 public:
  bool begin(const app::AppSettings& settings);
  void update(uint32_t nowMs);
  void setUsbConnected(bool connected, uint32_t nowMs);
  void updateSettings(const app::AppSettings& settings);
  bool enabled() const { return enabled_; }
  int adcPin() const { return adcPin_; }
  uint16_t rawAdc() const { return rawAdc_; }
  uint32_t adcMillivolts() const { return adcMillivolts_; }
  float instantVoltage() const { return instantVoltage_; }
  float voltage() const { return filteredVoltage_; }
  uint8_t percent() const { return percent_; }
  BatteryState state() const { return state_; }
  int16_t remainingMinutes() const;
  bool charging() const { return state_ == BatteryState::Charging; }
  bool usbConnected() const { return usbConnected_; }
  RuntimeEstimateQuality runtimeEstimateQuality() const {
    return runtimeEstimateQuality_;
  }
  uint32_t runtimeObservedMs() const { return runtimeObservedMs_; }
  float runtimeObservedDrop() const { return runtimeObservedDrop_; }
  String remainingTimeText() const;
  const char* trendText() const;
  String stateText() const;
  String statusText() const;
  String diagnosticText() const;

 private:
  static constexpr uint8_t kHistorySamples = 30;
  static constexpr uint32_t kPostUsbPercentHoldMs = 60000UL;
  static constexpr uint32_t kEarlyRuntimeWindowMs = 60000UL;
  static constexpr float kEarlyRuntimeDropPercent = 0.20f;
  float estimatePercent(float voltage) const;
  void completeSeries(uint32_t nowMs);
  void pushTrend(float voltage);
  void updateState();
  void updateRuntimeEstimate(uint32_t nowMs);

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
  float displayedPercent_ = 0.0f;
  bool percentInitialized_ = false;
  bool usbConnected_ = false;
  bool postUsbPercentHold_ = false;
  uint32_t usbDisconnectedAtMs_ = 0;
  float trend_[kHistorySamples] = {0};
  uint8_t trendCount_ = 0;
  uint8_t trendIndex_ = 0;
  BatteryState state_ = BatteryState::WarmingUp;
  uint32_t runtimeAnchorMs_ = 0;
  float runtimeAnchorPercent_ = 0.0f;
  uint32_t runtimeObservedMs_ = 0;
  float runtimeObservedDrop_ = 0.0f;
  float smoothedPercentPerHour_ = 0.0f;
  int16_t runtimeRemainingMinutes_ = 0;
  uint32_t runtimeEstimateAtMs_ = 0;
  bool runtimeEstimateReady_ = false;
  RuntimeEstimateQuality runtimeEstimateQuality_ =
      RuntimeEstimateQuality::Unavailable;
};
