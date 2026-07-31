#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class BatteryMonitor;
class ConfigSyncManager;
class DisplayManager;
class HallSensor;
class PhoneLinkManager;
class RideLogger;
class RideStateMachine;
class RideSyncManager;
class SpeedCalculator;
class SpeedTrendLed;
class StorageManager;
class TouchManager;
class UiApp;
class UsbMassStorageManager;

class DevMonitor {
 public:
  void attach(DisplayManager& display, TouchManager& touch,
             StorageManager& storage, UsbMassStorageManager& usb,
             HallSensor& sensor, SpeedCalculator& speed,
             SpeedTrendLed& speedTrend, RideStateMachine& ride,
             BatteryMonitor& battery, RideLogger& logger,
             PhoneLinkManager& phone, UiApp& ui, RideSyncManager& rideSync,
             ConfigSyncManager& configSync);
  bool start();
  void stop();
  void noteLoopStart(uint32_t nowUs);
  void update(uint32_t nowMs);
  bool active() const { return active_; }
  bool hostConnected() const;
  uint32_t samplesEmitted() const { return sequence_; }
  uint32_t droppedDocuments() const { return droppedDocuments_; }
  uint32_t maximumLoopPeriodUs() const { return maximumLoopPeriodEverUs_; }
  uint32_t lastReportMs() const { return lastReportMs_; }

 private:
  void emitBoot();
  void emitSample(uint32_t nowMs, bool requested = false,
                  uint32_t requestId = 0);
  void processInput(uint32_t nowMs);
  void handleCommand(const char* input, size_t length, uint32_t nowMs);
  void beginResponse(uint32_t requestId, const char* command, bool ok);
  void sendErrorResponse(uint32_t requestId, const char* command,
                         const char* error);
  void writeDocument();

  static constexpr uint32_t kDefaultReportIntervalMs = 2000;
  static constexpr uint32_t kMinimumReportIntervalMs = 250;
  static constexpr uint32_t kMaximumReportIntervalMs = 10000;
  static constexpr size_t kDocumentCapacity = 6144;
  static constexpr size_t kLineCapacity = 6144;
  static constexpr size_t kCommandCapacity = 1024;
  static constexpr size_t kInputCapacity = 768;

  DisplayManager* display_ = nullptr;
  TouchManager* touch_ = nullptr;
  StorageManager* storage_ = nullptr;
  UsbMassStorageManager* usb_ = nullptr;
  HallSensor* sensor_ = nullptr;
  SpeedCalculator* speed_ = nullptr;
  SpeedTrendLed* speedTrend_ = nullptr;
  RideStateMachine* ride_ = nullptr;
  BatteryMonitor* battery_ = nullptr;
  RideLogger* logger_ = nullptr;
  PhoneLinkManager* phone_ = nullptr;
  UiApp* ui_ = nullptr;
  RideSyncManager* rideSync_ = nullptr;
  ConfigSyncManager* configSync_ = nullptr;

  StaticJsonDocument<kDocumentCapacity> document_;
  StaticJsonDocument<kCommandCapacity> commandDocument_;
  char line_[kLineCapacity] = {};
  char input_[kInputCapacity] = {};
  size_t inputLength_ = 0;
  uint32_t sequence_ = 0;
  uint32_t reportIntervalMs_ = kDefaultReportIntervalMs;
  uint32_t lastReportMs_ = 0;
  uint32_t intervalStartedMs_ = 0;
  uint32_t lastLoopStartedUs_ = 0;
  uint32_t lastLoopPeriodUs_ = 0;
  uint32_t maximumLoopPeriodUs_ = 0;
  uint32_t maximumLoopPeriodEverUs_ = 0;
  uint32_t intervalLoopCount_ = 0;
  uint32_t droppedDocuments_ = 0;
  bool active_ = false;
  bool bootPending_ = false;
  bool streamEnabled_ = true;
  bool inputOverflow_ = false;
  bool usbStoragePending_ = false;
  uint32_t usbStorageRequestedMs_ = 0;
};
