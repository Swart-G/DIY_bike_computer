#pragma once

#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>

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
  const String& lastStatus() const { return lastStatus_; }
  SdTestResult runSdTest();
  bool ensureReadyForIo(const char* context, String& error);
  bool ensureDirectory(const char* path, String& error);
  bool createDirectory(const char* path, String& error);

  bool loadSettings(app::AppSettings& settings, String& error);
  bool saveSettings(const app::AppSettings& settings, String& error);

  bool saveRecovery(const RideRecoveryData& recovery, String& error);
  bool loadRecovery(RideRecoveryData& recovery, String& error);
  bool clearRecovery(String& error);
  bool writeJsonAtomic(const char* path, JsonDocument& document, String& error);
  bool readJson(const char* path, JsonDocument& document, String& error);
  bool removePath(const char* path, String& error);
  bool recoverIoFailure(const char* context);
  void reportIoFailure(const char* context);
  uint32_t activeFrequencyHz() const { return activeFrequencyHz_; }
  uint32_t ioRecoveryCount() const { return ioRecoveryCount_; }
  bool takeDisplayResetRequest();

 private:
  bool ensureDir(const char* path, String* error = nullptr);
  bool tryBeginSd(SPIClass& spi, uint32_t frequency, bool formatIfEmpty,
                  bool isolateDisplay = false);
  bool writeTextFile(const char* path, const String& content, String& error);
  bool readTextFile(const char* path, String& content, String& error);

  uint32_t activeFrequencyHz_ = 0;
  bool formattedOnMount_ = false;
  bool sdAvailable_ = false;
  bool continueWithoutSaving_ = false;
  bool usbModeActive_ = false;
  bool displayResetRequest_ = false;
  uint32_t ioRecoveryCount_ = 0;
  String lastStatus_;
};
