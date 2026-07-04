#pragma once

#include <Arduino.h>
#include <SD.h>

#include "config/app_config.h"
#include "speed/RideStateMachine.h"

struct SdTestResult {
  bool ok = false;
  String message;
  String readBack;
};

class StorageManager {
 public:
  bool begin();
  bool retry();
  void continueWithoutSaving();

  bool sdAvailable() const { return sdAvailable_; }
  bool loggingEnabled() const { return sdAvailable_ && !usbModeActive_ && !continueWithoutSaving_; }
  bool usbModeActive() const { return usbModeActive_; }
  void setUsbModeActive(bool active) { usbModeActive_ = active; }

  String statusText() const;
  String sdInfoText() const;
  SdTestResult runSdTest();

  bool loadSettings(app::AppSettings& settings, String& error);
  bool saveSettings(const app::AppSettings& settings, String& error);

  bool saveRecovery(const RideRecoveryData& recovery, String& error);
  bool loadRecovery(RideRecoveryData& recovery, String& error);
  bool clearRecovery(String& error);

 private:
  bool ensureDir(const char* path);
  bool writeTextFile(const char* path, const String& content, String& error);
  bool readTextFile(const char* path, String& content, String& error);

  uint32_t activeFrequencyHz_ = 0;
  bool sdAvailable_ = false;
  bool continueWithoutSaving_ = false;
  bool usbModeActive_ = false;
};
