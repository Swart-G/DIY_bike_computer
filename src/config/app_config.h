#pragma once

#include <Arduino.h>
#include <strings.h>

namespace app {

static constexpr const char* FIRMWARE_VERSION = "0.1.0-test";
static constexpr const char* BOARD_NAME = "ESP32-S3-N16R8";
static constexpr const char* DISPLAY_NAME = "ST7796";
static constexpr const char* TOUCH_NAME = "FT6336";

static constexpr const char* SD_TEST_FILE = "/BIKE_SPEEDOMETER_SD_TEST.txt";
static constexpr const char* CONFIG_FILE = "/config/bike_config.json";
static constexpr const char* RECOVERY_FILE = "/state/current_ride.json";
static constexpr const char* RECOVERY_TMP_FILE = "/state/current_ride.tmp";

static constexpr const char* SD_TEST_CONTENT =
    "Bike Speedometer SD test\n"
    "If you can read this file, SD write/read works.\n"
    "Device: ESP32-S3-N16R8\n"
    "Display: ST7796\n"
    "Touch: FT6336\n";

struct AppSettings {
  float wheelCircumferenceM = 2.194f;
  float stopThresholdKmh = 3.0f;
  uint32_t uiUpdateIntervalMs = 200;
  uint32_t logSampleIntervalMs = 1000;
  uint32_t graphWindowSeconds = 60;
  uint8_t displayBrightnessPercent = 80;

  bool sensorPullupEnabled = true;
  int sensorActiveLevel = LOW;
  int sensorInterruptMode = FALLING;
  uint32_t minPulseIntervalMs = 50;

  bool batteryMonitorEnabled = false;
  int batteryAdcPin = -1;
};

inline const char* levelToString(int level) {
  return level == HIGH ? "HIGH" : "LOW";
}

inline const char* interruptModeToString(int mode) {
  switch (mode) {
    case RISING:
      return "RISING";
    case CHANGE:
      return "CHANGE";
    case FALLING:
    default:
      return "FALLING";
  }
}

inline int levelFromString(const char* value, int fallback) {
  if (!value) {
    return fallback;
  }
  if (strcasecmp(value, "HIGH") == 0) {
    return HIGH;
  }
  if (strcasecmp(value, "LOW") == 0) {
    return LOW;
  }
  return fallback;
}

inline int interruptModeFromString(const char* value, int fallback) {
  if (!value) {
    return fallback;
  }
  if (strcasecmp(value, "RISING") == 0) {
    return RISING;
  }
  if (strcasecmp(value, "CHANGE") == 0) {
    return CHANGE;
  }
  if (strcasecmp(value, "FALLING") == 0) {
    return FALLING;
  }
  return fallback;
}

inline void validateSettings(AppSettings& settings) {
  if (settings.wheelCircumferenceM < 0.5f || settings.wheelCircumferenceM > 3.5f) {
    settings.wheelCircumferenceM = 2.194f;
  }
  if (settings.stopThresholdKmh < 0.5f || settings.stopThresholdKmh > 15.0f) {
    settings.stopThresholdKmh = 3.0f;
  }
  if (settings.uiUpdateIntervalMs < 50 || settings.uiUpdateIntervalMs > 2000) {
    settings.uiUpdateIntervalMs = 200;
  }
  if (settings.logSampleIntervalMs < 250 || settings.logSampleIntervalMs > 10000) {
    settings.logSampleIntervalMs = 1000;
  }
  if (settings.graphWindowSeconds < 10 || settings.graphWindowSeconds > 300) {
    settings.graphWindowSeconds = 60;
  }
  if (settings.displayBrightnessPercent > 100) {
    settings.displayBrightnessPercent = 80;
  }
  if (settings.minPulseIntervalMs < 10 || settings.minPulseIntervalMs > 1000) {
    settings.minPulseIntervalMs = 50;
  }
  if (settings.sensorInterruptMode != FALLING && settings.sensorInterruptMode != RISING &&
      settings.sensorInterruptMode != CHANGE) {
    settings.sensorInterruptMode = FALLING;
  }
  settings.batteryMonitorEnabled = false;
  settings.batteryAdcPin = -1;
}

}  // namespace app
