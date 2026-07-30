#include "storage/StorageManager.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <cerrno>
#include <cstring>

#include "bus/SharedSpiBus.h"
#include "config/hardware_config.h"

namespace {

class SdBusGuard {
 public:
  SdBusGuard() : bus_(true) {}

 private:
  hw::SharedSpiBusGuard bus_;
};

String errnoMessage(const char* operation, int errorCode) {
  String message(operation);
  message += " errno=";
  message += String(errorCode);
  if (errorCode != 0) {
    message += " (";
    message += strerror(errorCode);
    message += ")";
  }
  return message;
}

enum class DirectoryCreateResult : uint8_t {
  Created,
  AlreadyExists,
  IoFailure,
};

}

bool StorageManager::begin() {
  continueWithoutSaving_ = false;
  usbModeActive_ = false;

  hw::configureSharedSpiChipSelects();

  sdAvailable_ = false;
  activeFrequencyHz_ = 0;
  formattedOnMount_ = false;
  lastStatus_ = "SD init failed";

  SPIClass& spi = TFT_eSPI::getSPIinstance();
  if (tryBeginSd(spi, hw::SD_SAFE_SPI_FREQUENCY_HZ, false) ||
      tryBeginSd(spi, hw::SD_SAFE_SPI_FREQUENCY_HZ, false, true)) {
    sdAvailable_ = true;
    activeFrequencyHz_ = hw::SD_SAFE_SPI_FREQUENCY_HZ;
  }

  if (sdAvailable_) {
    lastStatus_ = String("SD mounted at ") +
                  String(activeFrequencyHz_ / 1000UL) + " kHz";
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
  text += "SPI kHz: " + String(activeFrequencyHz_ / 1000UL) + "\n";
  text += "I/O recoveries: " + String(ioRecoveryCount_) + "\n";
  if (formattedOnMount_) {
    text += "FS repaired on mount\n";
  }
  return text;
}

SdTestResult StorageManager::runSdTest() {
  SdTestResult result;
  const uint32_t recoveryCountBefore = ioRecoveryCount_;
  if (usbModeActive_) {
    result.message = "USB Mass Storage is active. SD test blocked.";
    return result;
  }
  if (!sdAvailable_) {
    result.message = "SD card not found.";
    return result;
  }

  String preflightError;
  if (!ensureReadyForIo("SD test preflight", preflightError)) {
    result.message = "SD preflight failed:\n" + preflightError;
    return result;
  }

  String error;
  for (uint8_t cycle = 1; cycle <= 3; ++cycle) {
    if (!writeTextFile(app::SD_TEST_FILE, app::SD_TEST_CONTENT, error)) {
      result.message = "Cycle " + String(cycle) +
                       " root write failed:\n" + error;
      return result;
    }
    if (!readTextFile(app::SD_TEST_FILE, result.readBack, error)) {
      result.message = "Cycle " + String(cycle) +
                       " root read failed:\n" + error;
      return result;
    }
    if (result.readBack != app::SD_TEST_CONTENT) {
      result.message = "Cycle " + String(cycle) + " read text mismatch";
      return result;
    }
  }

  String directoryError;
  if (!ensureDir("/config", &directoryError)) {
    result.readBack = "Root file write/read: OK\n";
    result.message = "Directory test failed:\n" + directoryError;
    return result;
  }

  result.ok = true;
  result.message =
      ioRecoveryCount_ == recoveryCountBefore
          ? "3x write/read + directory: OK"
          : "3x OK after recovery; check wiring";
  return result;
}

bool StorageManager::loadSettings(app::AppSettings& settings, String& error) {
  Preferences prefs;
  prefs.begin("bike", true);
  settings.wheelCircumferenceM = prefs.getFloat("wheel_m", settings.wheelCircumferenceM);
  settings.pulsesPerRevolution = prefs.getUChar("ppr", settings.pulsesPerRevolution);
  settings.stopThresholdKmh = prefs.getFloat("stop_kmh", settings.stopThresholdKmh);
  settings.autoPauseEnabled = prefs.getBool("auto_pause", settings.autoPauseEnabled);
  settings.autoPauseDelayMs = prefs.getUInt("auto_delay", settings.autoPauseDelayMs);
  settings.logSampleIntervalMs = prefs.getUInt("log_ms", settings.logSampleIntervalMs);
  settings.graphWindowSeconds = prefs.getUInt("graph_s", settings.graphWindowSeconds);
  settings.displayBrightnessPercent = prefs.getUChar("bright", settings.displayBrightnessPercent);
  settings.rgbSpeedTrendEnabled = prefs.getBool("rgb_enabled", settings.rgbSpeedTrendEnabled);
  settings.rgbSpeedTrendToleranceKmh = prefs.getFloat("rgb_tol", settings.rgbSpeedTrendToleranceKmh);
  settings.rgbSpeedTrendTolerance5sKmh =
      prefs.getFloat("rgb_tol5", settings.rgbSpeedTrendTolerance5sKmh);
  settings.rgbSpeedTrendTolerance10sKmh =
      prefs.getFloat("rgb_tol10", settings.rgbSpeedTrendTolerance10sKmh);
  settings.rgbLedBrightnessPercent = prefs.getUChar("rgb_bright", settings.rgbLedBrightnessPercent);
  settings.batteryCalibrationFactor = prefs.getFloat("bat_cal", settings.batteryCalibrationFactor);
  prefs.end();
  if (!sdAvailable_ || usbModeActive_) {
    app::validateSettings(settings);
    error = usbModeActive_ ? "SD owned by USB host; using NVS settings"
                           : "SD unavailable; using NVS settings";
    return true;
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

  StaticJsonDocument<1536> doc;
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
  settings.pulsesPerRevolution = doc["pulses_per_revolution"] | settings.pulsesPerRevolution;
  settings.stopThresholdKmh = doc["stop_threshold_kmh"] | settings.stopThresholdKmh;
  settings.autoPauseEnabled = doc["auto_pause_enabled"] | settings.autoPauseEnabled;
  settings.autoPauseDelayMs = doc["auto_pause_delay_ms"] | settings.autoPauseDelayMs;
  settings.maxPlausibleSpeedKmh = doc["max_plausible_speed_kmh"] | settings.maxPlausibleSpeedKmh;
  settings.uiUpdateIntervalMs = doc["ui_update_interval_ms"] | settings.uiUpdateIntervalMs;
  settings.logSampleIntervalMs = doc["log_sample_interval_ms"] | settings.logSampleIntervalMs;
  settings.graphWindowSeconds = doc["graph_window_seconds"] | settings.graphWindowSeconds;
  settings.displayBrightnessPercent = doc["display_brightness_percent"] | settings.displayBrightnessPercent;
  settings.recoveryIntervalMs = doc["recovery_interval_ms"] | settings.recoveryIntervalMs;

  JsonObject sensor = doc["sensor"];
  if (!sensor.isNull()) {
    settings.sensorPullupEnabled = sensor["pullup_enabled"] | settings.sensorPullupEnabled;
    settings.minPulseIntervalMs = sensor["min_pulse_interval_ms"] | settings.minPulseIntervalMs;
    settings.sensorActiveLevel =
        app::levelFromString(sensor["active_level"] | nullptr, settings.sensorActiveLevel);
    settings.sensorInterruptMode =
        app::interruptModeFromString(sensor["interrupt_edge"] | nullptr, settings.sensorInterruptMode);
  }

  JsonObject battery = doc["battery"];
  if (!battery.isNull()) {
    settings.batteryCalibrationFactor = battery["calibration_factor"] | settings.batteryCalibrationFactor;
    settings.batteryLowPercent = battery["low_percent"] | settings.batteryLowPercent;
    settings.batteryCriticalPercent = battery["critical_percent"] | settings.batteryCriticalPercent;
  }

  JsonObject rgbIndicator = doc["rgb_speed_indicator"];
  if (!rgbIndicator.isNull()) {
    settings.rgbSpeedTrendEnabled =
        rgbIndicator["enabled"] | settings.rgbSpeedTrendEnabled;
    settings.rgbSpeedTrendToleranceKmh =
        rgbIndicator["stable_tolerance_2s_kmh"] |
        (rgbIndicator["stable_tolerance_kmh"] |
         settings.rgbSpeedTrendToleranceKmh);
    settings.rgbSpeedTrendTolerance5sKmh =
        rgbIndicator["stable_tolerance_5s_kmh"] |
        settings.rgbSpeedTrendTolerance5sKmh;
    settings.rgbSpeedTrendTolerance10sKmh =
        rgbIndicator["stable_tolerance_10s_kmh"] |
        settings.rgbSpeedTrendTolerance10sKmh;
    settings.rgbLedBrightnessPercent =
        rgbIndicator["brightness_percent"] |
        settings.rgbLedBrightnessPercent;
  }

  app::validateSettings(settings);
  Preferences writePrefs; writePrefs.begin("bike", false);
  writePrefs.putFloat("wheel_m", settings.wheelCircumferenceM); writePrefs.putUChar("ppr", settings.pulsesPerRevolution);
  writePrefs.putFloat("stop_kmh", settings.stopThresholdKmh); writePrefs.putUChar("bright", settings.displayBrightnessPercent);
  writePrefs.putBool("auto_pause", settings.autoPauseEnabled);
  writePrefs.putUInt("auto_delay", settings.autoPauseDelayMs);
  writePrefs.putUInt("log_ms", settings.logSampleIntervalMs);
  writePrefs.putUInt("graph_s", settings.graphWindowSeconds);
  writePrefs.putBool("rgb_enabled", settings.rgbSpeedTrendEnabled);
  writePrefs.putFloat("rgb_tol", settings.rgbSpeedTrendToleranceKmh);
  writePrefs.putFloat("rgb_tol5", settings.rgbSpeedTrendTolerance5sKmh);
  writePrefs.putFloat("rgb_tol10", settings.rgbSpeedTrendTolerance10sKmh);
  writePrefs.putUChar("rgb_bright", settings.rgbLedBrightnessPercent);
  writePrefs.putFloat("bat_cal", settings.batteryCalibrationFactor); writePrefs.end();
  return true;
}

bool StorageManager::saveSettings(const app::AppSettings& settings, String& error) {
  if (usbModeActive_) {
    error = "SD owned by USB host";
    return false;
  }
  auto persistNvs = [&settings]() {
    Preferences p;
    p.begin("bike", false);
    p.putFloat("wheel_m", settings.wheelCircumferenceM);
    p.putUChar("ppr", settings.pulsesPerRevolution);
    p.putFloat("stop_kmh", settings.stopThresholdKmh);
    p.putBool("auto_pause", settings.autoPauseEnabled);
    p.putUInt("auto_delay", settings.autoPauseDelayMs);
    p.putUInt("log_ms", settings.logSampleIntervalMs);
    p.putUInt("graph_s", settings.graphWindowSeconds);
    p.putUChar("bright", settings.displayBrightnessPercent);
    p.putBool("rgb_enabled", settings.rgbSpeedTrendEnabled);
    p.putFloat("rgb_tol", settings.rgbSpeedTrendToleranceKmh);
    p.putFloat("rgb_tol5", settings.rgbSpeedTrendTolerance5sKmh);
    p.putFloat("rgb_tol10", settings.rgbSpeedTrendTolerance10sKmh);
    p.putUChar("rgb_bright", settings.rgbLedBrightnessPercent);
    p.putFloat("bat_cal", settings.batteryCalibrationFactor);
    p.end();
  };
  if (!sdAvailable_) {
    persistNvs();
    error = "Settings saved to NVS; SD unavailable";
    return true;
  }
  SdBusGuard bus;
  ensureDir("/config");

  StaticJsonDocument<1536> doc;
  doc["format_version"] = app::CONFIG_FORMAT_VERSION;
  doc["wheel_circumference_m"] = settings.wheelCircumferenceM;
  doc["pulses_per_revolution"] = settings.pulsesPerRevolution;
  doc["stop_threshold_kmh"] = settings.stopThresholdKmh;
  doc["auto_pause_enabled"] = settings.autoPauseEnabled;
  doc["auto_pause_delay_ms"] = settings.autoPauseDelayMs;
  doc["max_plausible_speed_kmh"] = settings.maxPlausibleSpeedKmh;
  doc["ui_update_interval_ms"] = settings.uiUpdateIntervalMs;
  doc["log_sample_interval_ms"] = settings.logSampleIntervalMs;
  doc["graph_window_seconds"] = settings.graphWindowSeconds;
  doc["display_brightness_percent"] = settings.displayBrightnessPercent;
  doc["recovery_interval_ms"] = settings.recoveryIntervalMs;

  JsonObject sensor = doc.createNestedObject("sensor");
  sensor["pin"] = hw::PIN_HALL_SENSOR;
  sensor["active_level"] = app::levelToString(settings.sensorActiveLevel);
  sensor["interrupt_edge"] = app::interruptModeToString(settings.sensorInterruptMode);
  sensor["pullup_enabled"] = settings.sensorPullupEnabled;
  sensor["min_pulse_interval_ms"] = settings.minPulseIntervalMs;

  JsonObject battery = doc.createNestedObject("battery");
  battery["enabled"] = true;
  battery["adc_pin"] = hw::PIN_BATTERY_ADC;
  battery["calibration_factor"] = settings.batteryCalibrationFactor;
  battery["low_percent"] = settings.batteryLowPercent;
  battery["critical_percent"] = settings.batteryCriticalPercent;

  JsonObject rgbIndicator = doc.createNestedObject("rgb_speed_indicator");
  rgbIndicator["enabled"] = settings.rgbSpeedTrendEnabled;
  rgbIndicator["pin"] = hw::PIN_RGB_LED;
  JsonArray comparisonWindows =
      rgbIndicator.createNestedArray("comparison_windows_ms");
  comparisonWindows.add(2000);
  comparisonWindows.add(5000);
  comparisonWindows.add(10000);
  rgbIndicator["stable_tolerance_2s_kmh"] =
      settings.rgbSpeedTrendToleranceKmh;
  rgbIndicator["stable_tolerance_5s_kmh"] =
      settings.rgbSpeedTrendTolerance5sKmh;
  rgbIndicator["stable_tolerance_10s_kmh"] =
      settings.rgbSpeedTrendTolerance10sKmh;
  rgbIndicator["brightness_percent"] = settings.rgbLedBrightnessPercent;
  const bool ok = writeJsonAtomic(app::CONFIG_FILE, doc, error);
  if (ok) persistNvs();
  return ok;
}

bool StorageManager::saveRecovery(const RideRecoveryData& recovery, String& error) {
  Preferences nvs; nvs.begin("ride_rcv", false);
  if (recovery.valid) {
    nvs.putBool("valid", true); nvs.putUInt("id", recovery.rideId); nvs.putString("folder", recovery.rideFolder);
    nvs.putFloat("dist", recovery.stats.distanceM); nvs.putFloat("max", recovery.stats.maxSpeedKmh);
    nvs.putULong64("elapsed", recovery.stats.elapsedMs); nvs.putULong64("record", recovery.stats.recordingMs);
    nvs.putULong64("moving", recovery.stats.movingMs); nvs.putULong64("pause", recovery.stats.pauseMs);
    nvs.putUInt("pulses", recovery.stats.pulseCount); nvs.putUInt("sample", recovery.lastSavedSampleIndex);
    nvs.putFloat("bat_start", recovery.batteryStartVoltage); nvs.putFloat("bat_min", recovery.batteryMinVoltage); nvs.putFloat("bat_max", recovery.batteryMaxVoltage);
  } else nvs.clear();
  nvs.end();
  if (!recovery.valid) {
    return clearRecovery(error);
  }
  if (!sdAvailable_ || usbModeActive_) {
    error = "Recovery checkpoint saved to NVS; SD unavailable or USB active";
    return true;
  }
  SdBusGuard bus;
  ensureDir("/state");

  StaticJsonDocument<768> doc;
  doc["format_version"] = app::RECOVERY_FORMAT_VERSION;
  doc["ride_id"] = recovery.rideId;
  doc["ride_folder"] = recovery.rideFolder;
  doc["last_saved_sample_index"] = recovery.lastSavedSampleIndex;
  doc["logging_gap"] = recovery.loggingGap;
  doc["battery_start_voltage"] = recovery.batteryStartVoltage;
  doc["battery_min_voltage"] = recovery.batteryMinVoltage;
  doc["battery_max_voltage"] = recovery.batteryMaxVoltage;
  doc["last_state"] = recovery.lastState == RideState::RIDING ? "RIDING" : "PAUSED";
  doc["distance_m"] = recovery.stats.distanceM;
  doc["max_speed_kmh"] = recovery.stats.maxSpeedKmh;
  doc["avg_speed_kmh"] = recovery.stats.averageMovingSpeedKmh;
  doc["moving_time_ms"] = recovery.stats.movingMs;
  doc["elapsed_time_ms"] = recovery.stats.elapsedMs;
  doc["pause_time_ms"] = recovery.stats.pauseMs;
  doc["pulse_count"] = recovery.stats.pulseCount;
  doc["rejected_pulse_count"] = recovery.stats.rejectedPulseCount;
  doc["recording_time_ms"] = recovery.stats.recordingMs;
  doc["stopped_time_ms"] = recovery.stats.stoppedMs;
  doc["last_saved_ms"] = millis();

  return writeJsonAtomic(app::RECOVERY_FILE, doc, error);
}

bool StorageManager::loadRecovery(RideRecoveryData& recovery, String& error) {
  recovery = RideRecoveryData();
  SdBusGuard bus;
  if (!sdAvailable_ || usbModeActive_ || !SD.exists(app::RECOVERY_FILE)) {
    Preferences nvs; nvs.begin("ride_rcv", true); const bool valid=nvs.getBool("valid",false);
    if(valid) { recovery.valid=true; recovery.rideId=nvs.getUInt("id",0); strlcpy(recovery.rideFolder,nvs.getString("folder","").c_str(),sizeof(recovery.rideFolder)); recovery.stats.distanceM=nvs.getFloat("dist",0); recovery.stats.maxSpeedKmh=nvs.getFloat("max",0); recovery.stats.elapsedMs=nvs.getULong64("elapsed",0); recovery.stats.recordingMs=nvs.getULong64("record",0); recovery.stats.movingMs=nvs.getULong64("moving",0); recovery.stats.pauseMs=nvs.getULong64("pause",0); recovery.stats.pulseCount=nvs.getUInt("pulses",0); recovery.lastSavedSampleIndex=nvs.getUInt("sample",0); recovery.batteryStartVoltage=nvs.getFloat("bat_start",0); recovery.batteryMinVoltage=nvs.getFloat("bat_min",0); recovery.batteryMaxVoltage=nvs.getFloat("bat_max",0); recovery.lastState=RideState::PAUSED; recovery.stats.stoppedMs=recovery.stats.recordingMs>recovery.stats.movingMs?recovery.stats.recordingMs-recovery.stats.movingMs:0; }
    nvs.end(); return valid;
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
  recovery.rideId = doc["ride_id"] | 0UL;
  strlcpy(recovery.rideFolder, doc["ride_folder"] | "", sizeof(recovery.rideFolder));
  recovery.lastSavedSampleIndex = doc["last_saved_sample_index"] | 0UL;
  recovery.loggingGap = doc["logging_gap"] | false;
  recovery.batteryStartVoltage = doc["battery_start_voltage"] | 0.0f;
  recovery.batteryMinVoltage = doc["battery_min_voltage"] | 0.0f;
  recovery.batteryMaxVoltage = doc["battery_max_voltage"] | 0.0f;
  recovery.lastState = strcmp(lastState, "RIDING") == 0 ? RideState::RIDING : RideState::PAUSED;
  recovery.stats.distanceM = doc["distance_m"] | 0.0f;
  recovery.stats.maxSpeedKmh = doc["max_speed_kmh"] | 0.0f;
  recovery.stats.averageMovingSpeedKmh = doc["avg_speed_kmh"] | 0.0f;
  recovery.stats.movingMs = doc["moving_time_ms"] | 0UL;
  recovery.stats.elapsedMs = doc["elapsed_time_ms"] | 0UL;
  recovery.stats.pauseMs = doc["pause_time_ms"] | 0UL;
  recovery.stats.pulseCount = doc["pulse_count"] | 0UL;
  recovery.stats.rejectedPulseCount = doc["rejected_pulse_count"] | 0UL;
  recovery.stats.recordingMs = doc["recording_time_ms"] | recovery.stats.movingMs;
  recovery.stats.stoppedMs = doc["stopped_time_ms"] | 0ULL;
  return recovery.valid;
}

bool StorageManager::clearRecovery(String& error) {
  Preferences nvs; nvs.begin("ride_rcv",false); nvs.clear(); nvs.end();
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

bool StorageManager::ensureDir(const char* path, String* error) {
  auto ensureOnce = [this, path, error]() {
    SdBusGuard bus;
    if (!sdAvailable_) {
      if (error) *error = "SD unavailable";
      return false;
    }
    if (SD.exists(path)) {
      errno = 0;
      File existing = SD.open(path, FILE_READ);
      const int errorCode = errno;
      if (!existing) {
        if (error) *error = errnoMessage("open existing path failed", errorCode);
        return false;
      }
      const bool isDirectory = existing.isDirectory();
      existing.close();
      if (!isDirectory && error) {
        *error = String(path) + " exists but is not a directory";
      }
      return isDirectory;
    }
    errno = 0;
    const bool created = SD.mkdir(path);
    const int errorCode = errno;
    if (!created && error) {
      *error = errnoMessage("mkdir failed", errorCode);
    }
    return created;
  };
  if (ensureOnce()) return true;
  const String firstError = error ? *error : String("mkdir");
  if (!recoverIoFailure(firstError.c_str())) return false;
  return ensureOnce();
}

bool StorageManager::ensureReadyForIo(const char* context, String& error) {
  error = "";
  if (!sdAvailable_ || usbModeActive_ || continueWithoutSaving_) {
    error = "SD unavailable or USB active";
    return false;
  }

  auto probeOnce = [this, &error]() {
    SdBusGuard bus;
    errno = 0;
    File root = SD.open("/", FILE_READ);
    const int errorCode = errno;
    if (!root) {
      error = errnoMessage("root probe failed", errorCode);
      return false;
    }
    const bool isDirectory = root.isDirectory();
    root.close();
    if (!isDirectory) {
      error = "SD root is not a directory";
      return false;
    }
    return true;
  };

  if (probeOnce()) {
    return true;
  }

  const String firstError = error;
  if (recoverIoFailure(context ? context : firstError.c_str()) &&
      probeOnce()) {
    return true;
  }

  if (error.length() == 0) {
    error = firstError;
  }
  reportIoFailure(error.c_str());
  return false;
}

bool StorageManager::ensureDirectory(const char* path, String& error) {
  error = "";
  return ensureDir(path, &error);
}

bool StorageManager::createDirectory(const char* path, String& error) {
  if (!sdAvailable_ || usbModeActive_ || continueWithoutSaving_) {
    error = "SD unavailable or USB active";
    return false;
  }

  auto createOnce = [this, path, &error]() {
    SdBusGuard bus;
    errno = 0;
    if (SD.exists(path)) {
      error = String(path) + " already exists";
      return DirectoryCreateResult::AlreadyExists;
    }

    errno = 0;
    if (SD.mkdir(path)) {
      return DirectoryCreateResult::Created;
    }

    const int errorCode = errno;
    error = errnoMessage("mkdir failed", errorCode);
    return errorCode == EEXIST ? DirectoryCreateResult::AlreadyExists
                               : DirectoryCreateResult::IoFailure;
  };

  DirectoryCreateResult result = createOnce();
  if (result == DirectoryCreateResult::Created) {
    return true;
  }
  if (result == DirectoryCreateResult::AlreadyExists) {
    return false;
  }

  const String firstError = error;
  if (recoverIoFailure(firstError.c_str())) {
    error = "";
    result = createOnce();
    if (result == DirectoryCreateResult::Created) {
      return true;
    }
    if (result == DirectoryCreateResult::AlreadyExists) {
      return false;
    }
  }
  reportIoFailure(error.length() ? error.c_str() : firstError.c_str());
  return false;
}

bool StorageManager::tryBeginSd(SPIClass& spi, uint32_t frequency,
                                bool formatIfEmpty, bool isolateDisplay) {
  SdBusGuard bus;
  if (isolateDisplay) {
    pinMode(hw::PIN_LCD_RST, OUTPUT);
    digitalWrite(hw::PIN_LCD_CS, HIGH);
    digitalWrite(hw::PIN_SD_CS, HIGH);
    digitalWrite(hw::PIN_LCD_RST, LOW);
    delay(20);
    displayResetRequest_ = true;
  }

  hw::abortSdTransfer();
  SD.end();
  spi.end();
  delay(2);
  spi.begin(hw::PIN_SPI_SCK, hw::PIN_SPI_MISO, hw::PIN_SPI_MOSI, -1);
  // Some cards need a real quiet interval after CMD0/unmount before accepting
  // the next initialization. The Arduino core also sends 160 idle clocks, but
  // those happen immediately inside SD.begin(); giving the card time first
  // avoids intermittent STA_NOINIT/ENODEV on the very first filesystem call.
  hw::releaseSharedSpiDevices();
  delay(20);
  hw::clockSdCardIdle(20);
  delay(2);
  pinMode(hw::PIN_SPI_MISO, INPUT_PULLUP);
  delayMicroseconds(50);
  const bool mounted =
      SD.begin(hw::PIN_SD_CS, spi, frequency, "/sd",
               hw::SD_MAX_OPEN_FILES, formatIfEmpty);
  if (isolateDisplay) {
    digitalWrite(hw::PIN_LCD_RST, HIGH);
    delay(120);
  }
  if (mounted) {
    return true;
  }
  lastStatus_ = String("SD mount failed at ") +
                String(frequency / 1000UL) + " kHz";
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
  auto writeOnce = [this, path, &content, &error]() {
    SdBusGuard bus;
    errno = 0;
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
      const int errorCode = errno;
      error = errnoMessage("open write failed", errorCode);
      return false;
    }
    errno = 0;
    const size_t written = file.print(content);
    const int errorCode = errno;
    file.flush();
    file.close();
    if (written != content.length()) {
      error = errnoMessage("short write", errorCode);
      return false;
    }
    return true;
  };

  if (writeOnce()) return true;
  const String firstError = error;
  if (recoverIoFailure(firstError.c_str())) {
    error = "";
    if (writeOnce()) return true;
  }
  reportIoFailure(error.length() ? error.c_str() : firstError.c_str());
  return false;
}

bool StorageManager::readTextFile(const char* path, String& content, String& error) {
  content = "";
  if (!sdAvailable_ || usbModeActive_) {
    error = "SD unavailable or USB active";
    return false;
  }
  auto readOnce = [this, path, &content, &error]() {
    SdBusGuard bus;
    errno = 0;
    File file = SD.open(path, FILE_READ);
    if (!file) {
      const int errorCode = errno;
      error = errnoMessage("open read failed", errorCode);
      return false;
    }
    content = "";
    while (file.available()) {
      content += static_cast<char>(file.read());
      if (content.length() > 2048) {
        break;
      }
    }
    file.close();
    return true;
  };

  if (readOnce()) return true;
  const String firstError = error;
  if (recoverIoFailure(firstError.c_str())) {
    error = "";
    if (readOnce()) return true;
  }
  reportIoFailure(error.length() ? error.c_str() : firstError.c_str());
  return false;
}

bool StorageManager::writeJsonAtomic(const char* path, JsonDocument& document, String& error) {
  if (!sdAvailable_ || usbModeActive_) { error = "SD unavailable or owned by USB"; return false; }
  auto writeOnce = [this, path, &document, &error]() {
    char tmp[96];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    SdBusGuard bus;
    if (SD.exists(tmp) && !SD.remove(tmp)) {
      error = "Cannot remove stale JSON tmp";
      return false;
    }
    File f = SD.open(tmp, FILE_WRITE);
    if (!f) {
      error = "Cannot open JSON tmp";
      return false;
    }
    if (serializeJsonPretty(document, f) == 0) {
      f.close();
      error = "JSON write failed";
      return false;
    }
    f.flush();
    f.close();
    if (SD.exists(path) && !SD.remove(path)) {
      error = "Cannot replace JSON";
      return false;
    }
    if (!SD.rename(tmp, path)) {
      error = "Cannot commit JSON";
      return false;
    }
    return true;
  };

  if (writeOnce()) return true;
  const String firstError = error;
  if (recoverIoFailure(firstError.c_str())) {
    error = "";
    if (writeOnce()) return true;
  }
  reportIoFailure(error.length() ? error.c_str() : firstError.c_str());
  return false;
}
bool StorageManager::readJson(const char* path, JsonDocument& document, String& error) {
  if (!sdAvailable_ || usbModeActive_) { error = "SD unavailable"; return false; }
  SdBusGuard bus; File f=SD.open(path, FILE_READ); if(!f) { error="Cannot open JSON"; return false; }
  DeserializationError e=deserializeJson(document,f); f.close(); if(e) { error=e.c_str(); return false; } return true;
}
bool StorageManager::removePath(const char* path, String& error) { if(!sdAvailable_ || usbModeActive_) { error="SD unavailable"; return false; } SdBusGuard bus; if(SD.exists(path) && !SD.remove(path)) { error="Cannot delete"; return false; } return true; }

bool StorageManager::recoverIoFailure(const char* context) {
  if (usbModeActive_ || continueWithoutSaving_) return false;

  sdAvailable_ = false;
  Serial.print("[SD] I/O failure: ");
  Serial.print(context ? context : "unknown");
  Serial.println("; remounting at 400 kHz");

  SPIClass& spi = TFT_eSPI::getSPIinstance();
  constexpr uint16_t kRetryDelaysMs[] = {25, 100, 250};
  for (uint8_t attempt = 0;
       attempt < sizeof(kRetryDelaysMs) / sizeof(kRetryDelaysMs[0]);
       ++attempt) {
    delay(kRetryDelaysMs[attempt]);
    const bool isolateDisplay = attempt > 0;
    if (tryBeginSd(spi, hw::SD_SAFE_SPI_FREQUENCY_HZ, false,
                   isolateDisplay)) {
      sdAvailable_ = true;
      activeFrequencyHz_ = hw::SD_SAFE_SPI_FREQUENCY_HZ;
      ++ioRecoveryCount_;
      lastStatus_ = String("SD recovered at 400 kHz (#") +
                    String(ioRecoveryCount_) + ")";
      Serial.print("[SD] runtime recovery successful at 400 kHz, attempt ");
      Serial.println(attempt + 1);
      return true;
    }
  }

  lastStatus_ = String("SD ERROR: ") +
                (context ? context : "recovery failed");
  return false;
}

bool StorageManager::takeDisplayResetRequest() {
  const bool requested = displayResetRequest_;
  displayResetRequest_ = false;
  return requested;
}

void StorageManager::reportIoFailure(const char* context) {
  sdAvailable_ = false;
  lastStatus_ = String("SD ERROR: ") + context;
  SdBusGuard bus;
  SD.end();
}
