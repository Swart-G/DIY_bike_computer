#include "led/SpeedTrendLed.h"

#include <esp32-hal-rgb-led.h>
#include <math.h>

#include "config/hardware_config.h"

void SpeedTrendLed::begin(const app::AppSettings& settings) {
  resetHistory();
  if (settings.rgbSpeedTrendEnabled) {
    show(SpeedTrendState::Stable, settings.rgbLedBrightnessPercent);
  } else {
    off();
  }
}

void SpeedTrendLed::update(float speedKmh,
                           const app::AppSettings& settings,
                           uint32_t nowMs) {
  if (!settings.rgbSpeedTrendEnabled) {
    if (ledOn_) off();
    resetHistory();
    return;
  }

  if (!isfinite(speedKmh) || speedKmh < 0.0f) speedKmh = 0.0f;
  if (sampleCount_ == 0 ||
      static_cast<uint32_t>(nowMs - lastSampleMs_) >= SAMPLE_INTERVAL_MS) {
    appendSample(speedKmh, nowMs);
  }

  float baselineKmh = speedKmh;
  const SpeedTrendState next =
      baselineSample(nowMs, baselineKmh)
          ? speedtrend::classify(speedKmh, baselineKmh,
                                 settings.rgbSpeedTrendToleranceKmh)
          : SpeedTrendState::Stable;
  if (!ledOn_ || next != state_ ||
      shownBrightnessPercent_ != settings.rgbLedBrightnessPercent) {
    show(next, settings.rgbLedBrightnessPercent);
  }
}

void SpeedTrendLed::resetHistory() {
  writeIndex_ = 0;
  sampleCount_ = 0;
  lastSampleMs_ = 0;
  state_ = SpeedTrendState::Stable;
}

void SpeedTrendLed::appendSample(float speedKmh, uint32_t nowMs) {
  samples_[writeIndex_].timestampMs = nowMs;
  samples_[writeIndex_].speedKmh = speedKmh;
  writeIndex_ = (writeIndex_ + 1) % SAMPLE_CAPACITY;
  if (sampleCount_ < SAMPLE_CAPACITY) ++sampleCount_;
  lastSampleMs_ = nowMs;
}

bool SpeedTrendLed::baselineSample(uint32_t nowMs, float& speedKmh) const {
  if (sampleCount_ < 2) return false;
  for (uint8_t offset = 0; offset < sampleCount_; ++offset) {
    const uint8_t index =
        (writeIndex_ + SAMPLE_CAPACITY - 1 - offset) % SAMPLE_CAPACITY;
    const Sample& sample = samples_[index];
    if (static_cast<uint32_t>(nowMs - sample.timestampMs) >=
        COMPARISON_WINDOW_MS) {
      speedKmh = sample.speedKmh;
      return true;
    }
  }
  return false;
}

void SpeedTrendLed::show(SpeedTrendState state,
                         uint8_t brightnessPercent) {
  const uint8_t value =
      static_cast<uint8_t>((static_cast<uint16_t>(brightnessPercent) * 255U) /
                           100U);
  switch (state) {
    case SpeedTrendState::Accelerating:
      neopixelWrite(hw::PIN_RGB_LED, value, 0, value);
      break;
    case SpeedTrendState::Decelerating:
      neopixelWrite(hw::PIN_RGB_LED, value, 0, 0);
      break;
    case SpeedTrendState::Stable:
    default:
      neopixelWrite(hw::PIN_RGB_LED, 0, value, 0);
      break;
  }
  state_ = state;
  ledOn_ = true;
  shownBrightnessPercent_ = brightnessPercent;
}

void SpeedTrendLed::off() {
  neopixelWrite(hw::PIN_RGB_LED, 0, 0, 0);
  ledOn_ = false;
  shownBrightnessPercent_ = 0;
}
