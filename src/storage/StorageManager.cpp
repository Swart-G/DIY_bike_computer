#include "storage/StorageManager.h"

#include <ArduinoJson.h>
#include <TFT_eSPI.h>

#include "bus/SharedSpiBus.h"
#include "config/hardware_config.h"

namespace {

class SdBusGuard {
 public:
  SdBusGuard() : bus_(true) {}

 private:
  hw::SharedSpiBusGuard bus_;
};

}

bool StorageManager::begin() {
  continueWithoutSaving_ = false;
  usbModeActive_ = false;

  pinMode(hw::PIN_LCD_CS, OUTPUT);
  pinMode(hw::PIN_SD_CS, OUTPUT);
  hw::releaseSharedSpiDevices();

  const uint32_t frequencies[] = {hw::SD_SPI_FREQUENCY_HZ, 1000000UL};
  sdAvailable_ = false;
  activeFrequencyHz_ = 0;
  formattedOnMount_ = false;
  lastStatus_ = "SD init failed";

  SPIClass& spi = TFT_eSPI::getSPIinstance();
  for (uint32_t frequency : frequencies) {
    if (tryBeginSd(spi, frequency, false)) {
      sdAvailable_ = true;
      activeFrequencyHz_ = frequency;
      break;
    }
    if (tryBeginSd(spi, frequency, true)) {
      sdAvailable_ = true;
      activeFrequencyHz_ = frequency;
      formattedOnMount_ = true;
      break;
    }
  }

  if (sdAvailable_) {
    lastStatus_ = String("SD mounted at ") + String(activeFrequencyHz_ / 1000000UL) + " MHz";
    if (formattedOnMount_) {
      lastStatus_ += " after FAT repair";
    }
  }
  return sdAvailable_;
}

bool StorageManager::retry() {
  SdBusGuard bus;
  SD.end();
  return begin();
}

void StorageManager::continueWithoutSaving() {
  continueWithoutSaving_ = true;
  sdAvailable_ = false;
  SdBusGuard bus;
  SD.end();
}

String StorageManager::statusText() const {
  if (usbModeActive_) {
    return "USB MSC";
  }
  if (!sdAvailable_) {
    return lastStatus_.length() > 0 ? lastStatus_ : "NO SD";
  }
  if (!loggingEnabled()) {
    return "SD read-only";
  }
  return "SD OK";
}

String StorageManager::sdInfoText() const {
  if (!sdAvailable_) {
    return "SD card not available\nRide logging disabled\n";
  }

  SdBusGuard bus;
  String type = "UNKNOWN";
  switch (SD.cardType()) {
    case CARD_MMC:
      type = "MMC";
      break;
    case CARD_SD:
      type = "SDSC";
      break;
    case CARD_SDHC:
      type = "SDHC";
      break;
    case CARD_NONE:
      type = "NONE";
      break;
    default:
      break;
  }

  String text;
  text += "SD detected\n";
  text += "Type: " + type + "\n";
  text += "Card size MB: " + String(static_cast<uint32_t>(SD.cardSize() / (1024ULL * 1024ULL))) + "\n";
  text += "FS total MB: " + String(static_cast<uint32_t>(SD.totalBytes() / (1024ULL * 1024ULL))) + "\n";
  text += "FS used MB: " + String(static_cast<uint32_t>(SD.usedBytes() / (1024ULL * 1024ULL))) + "\n";
  text += "SPI MHz: " + String(activeFrequencyHz_ / 1000000UL) + "\n";
  if (formattedOnMount_) {
    text += "FS repaired on mount\n";
  }
  return text;
}

SdTestResult StorageManager::runSdTest() {
  SdTestResult result;
  if (usbModeActive_) {
    result.message = "USB Mass Storage is active. SD test blocked.";
    return result;
  }
  if (!sdAvailable_) {
    result.message = "SD card not found.";
    return result;
  }

  String error;
  if (!writeTextFile(app::SD_TEST_FILE, app::SD_TEST_CONTENT, error)) {
    result.message = "Write failed: " + error;
    return result;
  }
  if (!readTextFile(app::SD_TEST_FILE, result.readBack, error)) {
    result.message = "Read failed: " + error;
    return result;
  }
  result.ok = result.readBack == app::SD_TEST_CONTENT;
  result.message = result.ok ? "SD write/read test OK" : "Read text mismatch";
  return result;
}

bool StorageManager::loadSettings(app::AppSettings& settings, String& error) {
  if (!sdAvailable_ || usbModeActive_) {
    app::validateSettings(settings);
    return false;
  }

  SdBusGuard bus;
  if (!SD.exists(app::CONFIG_FILE)) {
    app::validateSettings(settings);
    return saveSettings(settings, error);
  }

  File file = SD.open(app::CONFIG_FILE, FILE_READ);
  if (!file) {
    error = "Cannot open config";
    app::validateSettings(settings);
    return false;
  }

  StaticJsonDocument<1024> doc;
  DeserializationError jsonError = deserializeJson(doc, file);
  file.close();
  if (jsonError) {
    error = String("Config parse error: ") + jsonError.c_str();
    app::validateSettings(settings);
    String saveError;
    saveSettings(settings, saveError);
    return false;
  }

  settings.wheelCircumferenceM = doc["wheel_circumference_m"] | settings.wheelCircumferenceM;
  settings.stopThresholdKmh = doc["stop_threshold_kmh"] | settings.stopThresholdKmh;
  settings.uiUpdateIntervalMs = doc["ui_update_interval_ms"] | settings.uiUpdateIntervalMs;
  settings.logSampleIntervalMs = doc["log_sample_interval_ms"] | settings.logSampleIntervalMs;
  settings.graphWindowSeconds = doc["graph_window_seconds"] | settings.graphWindowSeconds;
  settings.displayBrightnessPercent = doc["display_brightness_percent"] | settings.displayBrightnessPercent;

  JsonObject sensor = doc["sensor"];
  if (!sensor.isNull()) {
    settings.sensorPullupEnabled = sensor["pullup_enabled"] | settings.sensorPullupEnabled;
    settings.minPulseIntervalMs = sensor["min_pulse_interval_ms"] | settings.minPulseIntervalMs;
    settings.sensorActiveLevel =
        app::levelFromString(sensor["active_level"] | nullptr, settings.sensorActiveLevel);
    settings.sensorInterruptMode =
        app::interruptModeFromString(sensor["interrupt_edge"] | nullptr, settings.sensorInterruptMode);
  }

  app::validateSettings(settings);
  return true;
}

bool StorageManager::saveSettings(const app::AppSettings& settings, String& error) {
  if (!sdAvailable_ || usbModeActive_) {
    error = "SD unavailable or owned by USB host";
    return false;
  }
  SdBusGuard bus;
  ensureDir("/config");

  StaticJsonDocument<1024> doc;
  doc["version"] = 1;
  doc["wheel_circumference_m"] = settings.wheelCircumferenceM;
  doc["stop_threshold_kmh"] = settings.stopThresholdKmh;
  doc["ui_update_interval_ms"] = settings.uiUpdateIntervalMs;
  doc["log_sample_interval_ms"] = settings.logSampleIntervalMs;
  doc["graph_window_seconds"] = settings.graphWindowSeconds;
  doc["display_brightness_percent"] = settings.displayBrightnessPercent;

  JsonObject sensor = doc.createNestedObject("sensor");
  sensor["pin"] = hw::PIN_HALL_SENSOR;
  sensor["active_level"] = app::levelToString(settings.sensorActiveLevel);
  sensor["interrupt_edge"] = app::interruptModeToString(settings.sensorInterruptMode);
  sensor["pullup_enabled"] = settings.sensorPullupEnabled;
  sensor["min_pulse_interval_ms"] = settings.minPulseIntervalMs;

  JsonObject battery = doc.createNestedObject("battery");
  battery["enabled"] = false;
  battery["adc_pin"] = nullptr;

  if (SD.exists(app::CONFIG_FILE)) {
    SD.remove(app::CONFIG_FILE);
  }
  File file = SD.open(app::CONFIG_FILE, FILE_WRITE);
  if (!file) {
    error = "Cannot write config";
    return false;
  }
  serializeJsonPretty(doc, file);
  file.close();
  return true;
}

bool StorageManager::saveRecovery(const RideRecoveryData& recovery, String& error) {
  if (!recovery.valid) {
    return clearRecovery(error);
  }
  if (!sdAvailable_ || usbModeActive_) {
    error = "Recovery not saved: SD unavailable or USB active";
    return false;
  }
  SdBusGuard bus;
  ensureDir("/state");

  StaticJsonDocument<512> doc;
  doc["version"] = 1;
  doc["last_state"] = recovery.lastState == RideState::RIDING ? "RIDING" : "PAUSED";
  doc["distance_m"] = recovery.stats.distanceM;
  doc["max_speed_kmh"] = recovery.stats.maxSpeedKmh;
  doc["avg_speed_kmh"] = recovery.stats.avgSpeedKmh;
  doc["moving_time_ms"] = recovery.stats.movingMs;
  doc["elapsed_time_ms"] = recovery.stats.elapsedMs;
  doc["pause_time_ms"] = recovery.stats.pauseMs;
  doc["pulse_count"] = recovery.stats.pulseCount;
  doc["last_saved_ms"] = millis();

  if (SD.exists(app::RECOVERY_TMP_FILE)) {
    SD.remove(app::RECOVERY_TMP_FILE);
  }
  File tmp = SD.open(app::RECOVERY_TMP_FILE, FILE_WRITE);
  if (!tmp) {
    error = "Cannot write recovery tmp";
    return false;
  }
  serializeJsonPretty(doc, tmp);
  tmp.close();

  if (SD.exists(app::RECOVERY_FILE)) {
    SD.remove(app::RECOVERY_FILE);
  }
  if (!SD.rename(app::RECOVERY_TMP_FILE, app::RECOVERY_FILE)) {
    error = "Cannot rename recovery tmp";
    return false;
  }
  return true;
}

bool StorageManager::loadRecovery(RideRecoveryData& recovery, String& error) {
  recovery = RideRecoveryData();
  SdBusGuard bus;
  if (!sdAvailable_ || usbModeActive_ || !SD.exists(app::RECOVERY_FILE)) {
    return false;
  }

  File file = SD.open(app::RECOVERY_FILE, FILE_READ);
  if (!file) {
    error = "Cannot open recovery";
    return false;
  }

  StaticJsonDocument<512> doc;
  DeserializationError jsonError = deserializeJson(doc, file);
  file.close();
  if (jsonError) {
    error = String("Recovery parse error: ") + jsonError.c_str();
    return false;
  }

  const char* lastState = doc["last_state"] | "PAUSED";
  recovery.valid = strcmp(lastState, "RIDING") == 0 || strcmp(lastState, "PAUSED") == 0;
  recovery.lastState = strcmp(lastState, "RIDING") == 0 ? RideState::RIDING : RideState::PAUSED;
  recovery.stats.distanceM = doc["distance_m"] | 0.0f;
  recovery.stats.maxSpeedKmh = doc["max_speed_kmh"] | 0.0f;
  recovery.stats.avgSpeedKmh = doc["avg_speed_kmh"] | 0.0f;
  recovery.stats.movingMs = doc["moving_time_ms"] | 0UL;
  recovery.stats.elapsedMs = doc["elapsed_time_ms"] | 0UL;
  recovery.stats.pauseMs = doc["pause_time_ms"] | 0UL;
  recovery.stats.pulseCount = doc["pulse_count"] | 0UL;
  return recovery.valid;
}

bool StorageManager::clearRecovery(String& error) {
  if (!sdAvailable_ || usbModeActive_) {
    return true;
  }
  SdBusGuard bus;
  if (SD.exists(app::RECOVERY_TMP_FILE)) {
    SD.remove(app::RECOVERY_TMP_FILE);
  }
  if (SD.exists(app::RECOVERY_FILE) && !SD.remove(app::RECOVERY_FILE)) {
    error = "Cannot remove recovery";
    return false;
  }
  return true;
}

bool StorageManager::ensureDir(const char* path) {
  SdBusGuard bus;
  if (!sdAvailable_ || SD.exists(path)) {
    return true;
  }
  return SD.mkdir(path);
}

bool StorageManager::tryBeginSd(SPIClass& spi, uint32_t frequency, bool formatIfEmpty) {
  SdBusGuard bus;
  SD.end();
  if (SD.begin(hw::PIN_SD_CS, spi, frequency, "/sd", 5, formatIfEmpty)) {
    return true;
  }
  lastStatus_ = String("SD mount failed at ") + String(frequency / 1000000UL) + " MHz";
  if (formatIfEmpty) {
    lastStatus_ += " with FAT repair";
  }
  return false;
}

bool StorageManager::writeTextFile(const char* path, const String& content, String& error) {
  if (!sdAvailable_ || usbModeActive_) {
    error = "SD unavailable or USB active";
    return false;
  }
  SdBusGuard bus;
  if (SD.exists(path)) {
    SD.remove(path);
  }
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    error = "open write failed";
    return false;
  }
  const size_t written = file.print(content);
  file.close();
  if (written != content.length()) {
    error = "short write";
    return false;
  }
  return true;
}

bool StorageManager::readTextFile(const char* path, String& content, String& error) {
  content = "";
  if (!sdAvailable_ || usbModeActive_) {
    error = "SD unavailable or USB active";
    return false;
  }
  SdBusGuard bus;
  File file = SD.open(path, FILE_READ);
  if (!file) {
    error = "open read failed";
    return false;
  }
  while (file.available()) {
    content += static_cast<char>(file.read());
    if (content.length() > 2048) {
      break;
    }
  }
  file.close();
  return true;
}
