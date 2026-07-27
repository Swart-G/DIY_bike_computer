#include "battery/BatteryMonitor.h"

#include "config/hardware_config.h"

bool BatteryMonitor::begin() {
  enabled_ = hw::BATTERY_MONITOR_ENABLED && hw::BATTERY_ADC_PIN >= 0;
  adcPin_ = hw::BATTERY_ADC_PIN;
  state_ = enabled_ ? BatteryState::WarmingUp : BatteryState::NotConfigured;
  if (enabled_) {
    pinMode(adcPin_, INPUT);
  }
  return enabled_;
}

void BatteryMonitor::update(uint32_t nowMs) {
  if (!enabled_) {
    return;
  }
  if (lastSampleMs_ != 0 && nowMs - lastSampleMs_ < hw::BATTERY_SAMPLE_INTERVAL_MS) {
    return;
  }
  lastSampleMs_ = nowMs;

  const float voltage = readBatteryVoltage();
  if (filteredVoltage_ <= 0.01f) {
    filteredVoltage_ = voltage;
  } else {
    filteredVoltage_ = filteredVoltage_ * 0.80f + voltage * 0.20f;
  }
  pushSample(filteredVoltage_);
  updateState();
}

String BatteryMonitor::stateText() const {
  switch (state_) {
    case BatteryState::Charging:
      return "charging";
    case BatteryState::FullOrNearFull:
      return "full";
    case BatteryState::Discharging:
      return "discharging";
    case BatteryState::Stable:
      return "stable";
    case BatteryState::WarmingUp:
      return "measuring";
    case BatteryState::NotConfigured:
    default:
      return "not configured";
  }
}

String BatteryMonitor::statusText() const {
  if (!enabled_) {
    return "Battery: N/A";
  }
  return String(filteredVoltage_, 2) + " V " + stateText();
}

String BatteryMonitor::diagnosticText() const {
  if (!enabled_) {
    return "Battery monitor: disabled\nADC pin: not configured\n";
  }

  String text;
  text += "Battery monitor: enabled\n";
  text += "ADC pin: GPIO" + String(adcPin_) + "\n";
  text += "Voltage: " + String(filteredVoltage_, 3) + " V\n";
  text += "State: " + stateText() + "\n";
  text += "Samples: " + String(sampleCount_) + "/" + String(kHistorySamples) + "\n";
  if (sampleCount_ >= kCompareWindowSamples * 2) {
    const float nowAvg = averageWindow(0, kCompareWindowSamples);
    const float oldAvg = averageWindow(kCompareWindowSamples, kCompareWindowSamples);
    text += "Avg now: " + String(nowAvg, 3) + " V\n";
    text += "Avg prev: " + String(oldAvg, 3) + " V\n";
    text += "Delta: " + String(nowAvg - oldAvg, 3) + " V\n";
  } else {
    text += "State warmup until enough history\n";
  }
  return text;
}

float BatteryMonitor::readBatteryVoltage() const {
  const uint32_t millivolts = analogReadMilliVolts(adcPin_);
  float voltage = millivolts / 1000.0f;
  voltage *= hw::BATTERY_VOLTAGE_DIVIDER_RATIO;
  voltage *= hw::BATTERY_ADC_CALIBRATION_MULTIPLIER;
  voltage += hw::BATTERY_ADC_CALIBRATION_OFFSET_V;
  return voltage;
}

float BatteryMonitor::averageWindow(uint8_t newestOffset, uint8_t count) const {
  if (sampleCount_ == 0 || count == 0 || newestOffset >= sampleCount_) {
    return 0.0f;
  }
  if (newestOffset + count > sampleCount_) {
    count = sampleCount_ - newestOffset;
  }

  float sum = 0.0f;
  for (uint8_t i = 0; i < count; ++i) {
    const uint8_t offset = newestOffset + i;
    const uint8_t index = (writeIndex_ + kHistorySamples - 1 - offset) % kHistorySamples;
    sum += samples_[index];
  }
  return sum / count;
}

void BatteryMonitor::pushSample(float voltage) {
  samples_[writeIndex_] = voltage;
  writeIndex_ = (writeIndex_ + 1) % kHistorySamples;
  if (sampleCount_ < kHistorySamples) {
    ++sampleCount_;
  }
}

void BatteryMonitor::updateState() {
  if (sampleCount_ < kCompareWindowSamples * 2) {
    state_ = BatteryState::WarmingUp;
    return;
  }

  const float nowAvg = averageWindow(0, kCompareWindowSamples);
  const float oldAvg = averageWindow(kCompareWindowSamples, kCompareWindowSamples);
  const float delta = nowAvg - oldAvg;

  if (delta >= hw::BATTERY_CHARGE_DELTA_V) {
    state_ = BatteryState::Charging;
  } else if (nowAvg >= hw::BATTERY_FULL_VOLTAGE) {
    state_ = BatteryState::FullOrNearFull;
  } else if (delta <= -hw::BATTERY_CHARGE_DELTA_V) {
    state_ = BatteryState::Discharging;
  } else {
    state_ = BatteryState::Stable;
  }
}
