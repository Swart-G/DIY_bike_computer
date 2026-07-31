#pragma once

#include <Arduino.h>
#include <math.h>
#include <strings.h>

namespace app {

static constexpr const char* FIRMWARE_VERSION = "2.2.0";
static constexpr const char* BOARD_NAME = "ESP32-S3-N16R8";
static constexpr const char* DISPLAY_NAME = "ST7796";
static constexpr const char* TOUCH_NAME = "FT6336";
static constexpr uint8_t CONFIG_FORMAT_VERSION = 1;
static constexpr uint8_t RIDE_LOG_FORMAT_VERSION = 2;
static constexpr uint8_t RECOVERY_FORMAT_VERSION = 1;
// Version 2.0 has no user-facing brightness control. The legacy config field is kept
// in AppSettings so format-1 files continue to parse, but runtime always uses this value.
static constexpr uint8_t DISPLAY_FIXED_BRIGHTNESS_PERCENT = 80;

// Keep the diagnostic filename within 8.3 so the root write test does not
// depend on long-filename support or on any application directory.
static constexpr const char* SD_TEST_FILE = "/SDTEST.TXT";
static constexpr const char* CONFIG_FILE = "/config/bike_config.json";
static constexpr const char* RECOVERY_FILE = "/state/current_ride.json";
static constexpr const char* RECOVERY_TMP_FILE = "/state/current_ride.tmp";
static constexpr const char* RIDES_DIRECTORY = "/rides";

static constexpr const char* SD_TEST_CONTENT =
    "Bike Speedometer SD test\n"
    "If you can read this file, SD write/read works.\n"
    "Device: ESP32-S3-N16R8\n"
    "Display: ST7796\n"
    "Touch: FT6336\n";

struct AppSettings {
  float wheelCircumferenceM = 2.194f;
  uint8_t pulsesPerRevolution = 1;
  float stopThresholdKmh = 3.0f;
  bool autoPauseEnabled = true;
  uint32_t autoPauseDelayMs = 5000;
  float maxPlausibleSpeedKmh = 100.0f;
  uint32_t uiUpdateIntervalMs = 200;
  uint32_t logSampleIntervalMs = 1000;
  uint32_t recoveryIntervalMs = 15000;
  uint32_t graphWindowSeconds = 60;
  uint8_t displayBrightnessPercent = 80;
  bool rgbSpeedTrendEnabled = true;
  float rgbSpeedTrendToleranceKmh = 0.5f;
  float rgbSpeedTrendTolerance5sKmh = 0.5f;
  float rgbSpeedTrendTolerance10sKmh = 0.5f;
  uint8_t rgbLedBrightnessPercent = 20;

  bool sensorPullupEnabled = true;
  int sensorActiveLevel = LOW;
  int sensorInterruptMode = FALLING;
  uint32_t minPulseIntervalMs = 50;

  float batteryCalibrationFactor = 1.0f;
  uint8_t batteryLowPercent = 29;
  uint8_t batteryCriticalPercent = 15;
};

inline const char* levelToString(int level) { return level == HIGH ? "HIGH" : "LOW"; }
inline const char* interruptModeToString(int mode) {
  switch (mode) { case RISING: return "RISING"; case CHANGE: return "CHANGE"; default: return "FALLING"; }
}
inline int levelFromString(const char* value, int fallback) {
  if (!value) return fallback;
  if (strcasecmp(value, "HIGH") == 0) return HIGH;
  if (strcasecmp(value, "LOW") == 0) return LOW;
  return fallback;
}
inline int interruptModeFromString(const char* value, int fallback) {
  if (!value) return fallback;
  if (strcasecmp(value, "RISING") == 0) return RISING;
  if (strcasecmp(value, "CHANGE") == 0) return CHANGE;
  if (strcasecmp(value, "FALLING") == 0) return FALLING;
  return fallback;
}
inline void validateSettings(AppSettings& s) {
  if (!isfinite(s.wheelCircumferenceM) || s.wheelCircumferenceM < 0.5f || s.wheelCircumferenceM > 3.5f) s.wheelCircumferenceM = 2.194f;
  if (s.pulsesPerRevolution < 1 || s.pulsesPerRevolution > 16) s.pulsesPerRevolution = 1;
  if (!isfinite(s.stopThresholdKmh) || s.stopThresholdKmh < 0.5f || s.stopThresholdKmh > 15.0f) s.stopThresholdKmh = 3.0f;
  if (s.autoPauseDelayMs < 1000 || s.autoPauseDelayMs > 60000) s.autoPauseDelayMs = 5000;
  if (!isfinite(s.maxPlausibleSpeedKmh) || s.maxPlausibleSpeedKmh < 10.0f || s.maxPlausibleSpeedKmh > 150.0f) s.maxPlausibleSpeedKmh = 100.0f;
  if (s.uiUpdateIntervalMs < 50 || s.uiUpdateIntervalMs > 2000) s.uiUpdateIntervalMs = 200;
  if (s.logSampleIntervalMs < 250 || s.logSampleIntervalMs > 10000) s.logSampleIntervalMs = 1000;
  if (s.recoveryIntervalMs < 5000 || s.recoveryIntervalMs > 60000) s.recoveryIntervalMs = 15000;
  if (s.graphWindowSeconds < 10 || s.graphWindowSeconds > 300) s.graphWindowSeconds = 60;
  if (s.displayBrightnessPercent < 5 || s.displayBrightnessPercent > 100) s.displayBrightnessPercent = 80;
  if (!isfinite(s.rgbSpeedTrendToleranceKmh) || s.rgbSpeedTrendToleranceKmh < 0.1f || s.rgbSpeedTrendToleranceKmh > 5.0f) s.rgbSpeedTrendToleranceKmh = 0.5f;
  if (!isfinite(s.rgbSpeedTrendTolerance5sKmh) || s.rgbSpeedTrendTolerance5sKmh < 0.1f || s.rgbSpeedTrendTolerance5sKmh > 5.0f) s.rgbSpeedTrendTolerance5sKmh = 0.5f;
  if (!isfinite(s.rgbSpeedTrendTolerance10sKmh) || s.rgbSpeedTrendTolerance10sKmh < 0.1f || s.rgbSpeedTrendTolerance10sKmh > 5.0f) s.rgbSpeedTrendTolerance10sKmh = 0.5f;
  if (s.rgbLedBrightnessPercent < 5 || s.rgbLedBrightnessPercent > 100) s.rgbLedBrightnessPercent = 20;
  if (s.minPulseIntervalMs < 10 || s.minPulseIntervalMs > 2000) s.minPulseIntervalMs = 50;
  if (s.sensorInterruptMode != FALLING && s.sensorInterruptMode != RISING && s.sensorInterruptMode != CHANGE) s.sensorInterruptMode = FALLING;
  if (!isfinite(s.batteryCalibrationFactor) || s.batteryCalibrationFactor < 0.80f || s.batteryCalibrationFactor > 1.20f) s.batteryCalibrationFactor = 1.0f;
  if (s.batteryLowPercent < 16 || s.batteryLowPercent > 60) s.batteryLowPercent = 29;
  if (s.batteryCriticalPercent < 3 || s.batteryCriticalPercent >= s.batteryLowPercent) s.batteryCriticalPercent = 15;
}

}  // namespace app
