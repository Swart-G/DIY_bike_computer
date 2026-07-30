#include "battery/BatteryMonitor.h"

#include <algorithm>

#include "config/hardware_config.h"
#include "battery/BatteryMath.h"

bool BatteryMonitor::begin(const app::AppSettings& settings) {
  enabled_ = hw::BATTERY_MONITOR_ENABLED && hw::PIN_BATTERY_ADC >= 0;
  adcPin_ = hw::PIN_BATTERY_ADC;
  updateSettings(settings);
  if (!enabled_) return false;
  pinMode(adcPin_, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(adcPin_, ADC_11db);
  discardRemaining_ = hw::BATTERY_DISCARD_SAMPLES;
  state_ = BatteryState::WarmingUp;
  return true;
}

void BatteryMonitor::updateSettings(const app::AppSettings& settings) {
  calibrationFactor_ = settings.batteryCalibrationFactor;
  lowPercent_ = settings.batteryLowPercent;
  criticalPercent_ = settings.batteryCriticalPercent;
}

void BatteryMonitor::update(uint32_t nowMs) {
  if (!enabled_) return;
  if (lastSampleMs_ != 0 && nowMs - lastSampleMs_ < hw::BATTERY_SAMPLE_INTERVAL_MS) return;
  lastSampleMs_ = nowMs;
  const uint16_t raw = analogRead(adcPin_);
  const uint32_t mv = analogReadMilliVolts(adcPin_);
  rawAdc_ = raw;
  adcMillivolts_ = mv;
  if (discardRemaining_ > 0) { --discardRemaining_; return; }
  if (seriesCount_ < hw::BATTERY_SERIES_SAMPLES) seriesMv_[seriesCount_++] = static_cast<uint16_t>(mv);
  if (seriesCount_ == hw::BATTERY_SERIES_SAMPLES) completeSeries(nowMs);
}

void BatteryMonitor::completeSeries(uint32_t nowMs) {
  std::sort(seriesMv_, seriesMv_ + hw::BATTERY_SERIES_SAMPLES);
  uint32_t total = 0;
  for (uint8_t i = 1; i + 1 < hw::BATTERY_SERIES_SAMPLES; ++i) total += seriesMv_[i];
  const float adcV = (total / static_cast<float>(hw::BATTERY_SERIES_SAMPLES - 2)) / 1000.0f;
  instantVoltage_ = batterymath::calibratedVoltage(adcV, hw::BATTERY_VOLTAGE_DIVIDER_RATIO, calibrationFactor_);
  filteredVoltage_ = filteredVoltage_ < 0.1f ? instantVoltage_ : filteredVoltage_ * 0.90f + instantVoltage_ * 0.10f;
  const uint8_t estimated = static_cast<uint8_t>(roundf(estimatePercent(filteredVoltage_)));
  if (estimated > stablePercent_ + 1 || estimated + 1 < stablePercent_) stablePercent_ = estimated;
  percent_ = stablePercent_;
  pushTrend(filteredVoltage_);
  updateState();
  updateRuntimeEstimate(nowMs);
  seriesCount_ = 0;
  discardRemaining_ = hw::BATTERY_DISCARD_SAMPLES;
  seriesStartedMs_ = nowMs;
}

float BatteryMonitor::estimatePercent(float v) const { return batterymath::percentFromVoltage(v); }

void BatteryMonitor::pushTrend(float voltage) {
  trend_[trendIndex_] = voltage;
  trendIndex_ = (trendIndex_ + 1) % kHistorySamples;
  if (trendCount_ < kHistorySamples) ++trendCount_;
}

const char* BatteryMonitor::trendText() const {
  if (trendCount_ < kHistorySamples) return "UNKNOWN";
  const float oldest = trend_[trendIndex_];
  const float newest = trend_[(trendIndex_ + kHistorySamples - 1) % kHistorySamples];
  if (newest - oldest > hw::BATTERY_CHARGE_DELTA_V) return "CHARGING";
  if (oldest - newest > hw::BATTERY_CHARGE_DELTA_V) return "DISCHARGING";
  return "STABLE";
}

void BatteryMonitor::updateState() {
  if (trendCount_ < 3) { state_ = BatteryState::WarmingUp; return; }
  const char* trend = trendText();
  if (strcmp(trend, "CHARGING") == 0) state_ = BatteryState::Charging;
  else if (percent_ <= criticalPercent_) state_ = BatteryState::Critical;
  else if (percent_ <= lowPercent_) state_ = BatteryState::Low;
  else if (strcmp(trend, "DISCHARGING") == 0) state_ = BatteryState::Discharging;
  else state_ = BatteryState::Stable;
}

void BatteryMonitor::updateRuntimeEstimate(uint32_t nowMs) {
  if (state_ == BatteryState::Charging) {
    runtimeAnchorMs_ = 0;
    smoothedPercentPerHour_ = 0.0f;
    runtimeEstimateReady_ = false;
    return;
  }
  if (state_ == BatteryState::WarmingUp || percent_ == 0) return;

  if (runtimeAnchorMs_ == 0) {
    runtimeAnchorMs_ = nowMs;
    runtimeAnchorPercent_ = percent_;
    return;
  }
  if (percent_ > runtimeAnchorPercent_) {
    runtimeAnchorMs_ = nowMs;
    runtimeAnchorPercent_ = percent_;
    runtimeEstimateReady_ = false;
    return;
  }

  const uint32_t elapsedMs = nowMs - runtimeAnchorMs_;
  const uint8_t drop = runtimeAnchorPercent_ - percent_;
  if (elapsedMs < 5UL * 60UL * 1000UL || drop < 1) return;

  const float percentPerHour =
      drop * (3600000.0f / static_cast<float>(elapsedMs));
  if (percentPerHour < 0.15f || percentPerHour > 60.0f) {
    runtimeAnchorMs_ = nowMs;
    runtimeAnchorPercent_ = percent_;
    return;
  }
  smoothedPercentPerHour_ =
      smoothedPercentPerHour_ > 0.0f
          ? smoothedPercentPerHour_ * 0.75f + percentPerHour * 0.25f
          : percentPerHour;
  const int32_t remaining = static_cast<int32_t>(
      roundf((percent_ / smoothedPercentPerHour_) * 60.0f));
  runtimeRemainingMinutes_ =
      static_cast<int16_t>(constrain(remaining, 1L, 5999L));
  runtimeEstimateReady_ = true;
  runtimeEstimateAtMs_ = nowMs;
  runtimeAnchorMs_ = nowMs;
  runtimeAnchorPercent_ = percent_;
}

int16_t BatteryMonitor::remainingMinutes() const {
  if (!runtimeEstimateReady_) return -1;
  const uint32_t elapsedMinutes =
      (millis() - runtimeEstimateAtMs_) / 60000UL;
  return static_cast<int16_t>(
      max<int32_t>(1, runtimeRemainingMinutes_ -
                          static_cast<int32_t>(elapsedMinutes)));
}

String BatteryMonitor::remainingTimeText() const {
  if (!enabled_) return "--";
  if (charging()) return "charging";
  if (!runtimeEstimateReady_) return "calculating";
  const int16_t remaining = remainingMinutes();
  const uint16_t hours = remaining / 60;
  const uint8_t minutes = remaining % 60;
  return String("~ ") + String(hours) + "h " + String(minutes) + "m";
}

String BatteryMonitor::stateText() const {
  switch (state_) {
    case BatteryState::Low: return "low"; case BatteryState::Critical: return "critical";
    case BatteryState::Charging: return "charging"; case BatteryState::Discharging: return "discharging";
    case BatteryState::Stable: return "stable"; default: return "measuring";
  }
}
String BatteryMonitor::statusText() const {
  if (!enabled_) return "Battery: N/A";
  return String(percent_) + "% " + String(filteredVoltage_, 2) + "V";
}
String BatteryMonitor::diagnosticText() const {
  if (!enabled_) return "Battery monitor unavailable\n";
  String t = "GPIO" + String(adcPin_) + " / ADC1\nRaw ADC: " + String(rawAdc_) +
      "\nADC: " + String(adcMillivolts_) + " mV\nInstant: " + String(instantVoltage_, 3) +
      " V\nFiltered: " + String(filteredVoltage_, 3) + " V\nCalibration: " + String(calibrationFactor_, 3) +
      "\nEstimate: " + String(percent_) + "%\nRuntime: " +
      remainingTimeText() + "\nTrend: " + trendText() + "\nState: " +
      stateText();
  return t;
}
