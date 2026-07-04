#include "speed/SpeedCalculator.h"

void SpeedCalculator::reset() {
  currentKmh_ = 0.0f;
  lastPulseCount_ = 0;
  lastPulseMs_ = 0;
  lastIntervalMs_ = 0;
  hasValidInterval_ = false;
}

void SpeedCalculator::update(const HallSensorSnapshot& sensor, const app::AppSettings& settings,
                             uint32_t nowMs) {
  if (sensor.pulseCount != lastPulseCount_) {
    lastPulseCount_ = sensor.pulseCount;
    lastPulseMs_ = sensor.lastPulseMs;
    if (sensor.lastIntervalMs > 0) {
      lastIntervalMs_ = sensor.lastIntervalMs;
      currentKmh_ = speedFromInterval(lastIntervalMs_, settings.wheelCircumferenceM);
      hasValidInterval_ = true;
    }
    return;
  }

  if (!hasValidInterval_ || lastPulseMs_ == 0) {
    currentKmh_ = 0.0f;
    return;
  }

  const uint32_t sinceLastPulseMs = nowMs - lastPulseMs_;
  if (sinceLastPulseMs <= lastIntervalMs_) {
    return;
  }

  currentKmh_ = speedFromInterval(sinceLastPulseMs, settings.wheelCircumferenceM);
  if (currentKmh_ < settings.stopThresholdKmh) {
    currentKmh_ = 0.0f;
  }
}

float SpeedCalculator::speedFromInterval(uint32_t intervalMs, float wheelCircumferenceM) const {
  if (intervalMs == 0) {
    return 0.0f;
  }
  const float seconds = static_cast<float>(intervalMs) / 1000.0f;
  return (wheelCircumferenceM / seconds) * 3.6f;
}
