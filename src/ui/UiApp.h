#pragma once

#include <Arduino.h>

#include "battery/BatteryMonitor.h"
#include "display/DisplayManager.h"
#include "led/SpeedTrendLed.h"
#include "phone/PhoneLinkManager.h"
#include "rain/RainLockManager.h"
#include "speed/HallSensor.h"
#include "speed/RideStateMachine.h"
#include "speed/SpeedCalculator.h"
#include "storage/StorageManager.h"
#include "storage/RideLogger.h"
#include "storage/RideRepository.h"
#include "touch/TouchManager.h"
#include "ui/UiRouter.h"
#include "usb/UsbMassStorageManager.h"

class UiApp {
 public:
  void begin(DisplayManager& display, TouchManager& touch, StorageManager& storage,
             UsbMassStorageManager& usb, HallSensor& sensor, SpeedCalculator& speed,
             SpeedTrendLed& speedTrend,
             RideStateMachine& ride, BatteryMonitor& battery, RideLogger& logger,
             RideRepository& repository, PhoneLinkManager& phone,
             app::AppSettings& settings);
 void loop();
  bool rainLocked() const { return rainLock_.locked(); }

 private:
  using Screen = UiScreen;

  void enter(Screen screen);
  void updateModel(uint32_t nowMs);
  void handleTap(int16_t x, int16_t y);
  void handleRideTouchStart(int16_t x, int16_t y, uint32_t nowMs);
  void handleRideTouchMove(int16_t x, int16_t y);
  void handleRideTouchEnd(uint32_t nowMs);
  void handleHistoryTouchStart(int16_t x, int16_t y, uint32_t nowMs);
  void handleHistoryTouchMove(int16_t x, int16_t y);
  void handleHistoryTouchEnd(uint32_t nowMs);
  void handlePaint();
  void draw();

  void drawSdMissing();
  void drawRecovery();
  void drawMainMenu();
  void drawPhone();
  void drawHistory();
  void drawHistoryDetail();
  void drawDeleteRideConfirm();
  void drawDiagnostics();
  void drawDisplayTest();
  void drawTouchRawTest();
  void drawPaintTest();
  void drawSdTest();
  void drawUsbStorage();
  void drawSensorTest();
  void drawBatteryTest();
  void drawSystemInfo();
  void drawSettings();
  void drawSettingsRide();
  void drawSettingsDisplay();
  void drawSettingsSystem();
  void drawSettingsWheel();
  void drawSettingsStopThreshold();
  void drawSettingsAutoPauseDelay();
  void drawSettingsLogInterval();
  void drawSettingsRgbLed();
  void drawSettingsRgbStableRange();
  void drawSettingsRgbStableRange5s();
  void drawSettingsRgbStableRange10s();
  void drawSettingsRgbBrightness();
  void drawRide();
  void drawFinishConfirm();
  void drawRideSummary();

  void drawStatusBar(const String& title, const String& status = String());
  void drawStorageStatusIcon(int16_t x, int16_t y);
  void drawBatteryStatusIcon(int16_t x, int16_t y);
  void drawSoftButton(int16_t x, int16_t y, int16_t w, int16_t h, const String& label,
                      uint16_t accentColor, bool enabled = true, uint16_t fillColor = 0xFFFF);
  void drawBackButton();
  void drawTextBlock(const String& text, int16_t x, int16_t y, int16_t lineHeight,
                     uint16_t color = TFT_WHITE, uint16_t bg = TFT_BLACK, uint8_t font = 2);
  bool hit(int16_t x, int16_t y, int16_t bx, int16_t by, int16_t bw, int16_t bh) const;
  String durationText(uint64_t ms) const;
  String storageStatusShort() const;
  void startUsbMode();
  bool settingsLockedDuringRide() const;
  void showSettingsLockedNotice();
  bool commitSettings(const app::AppSettings& candidate,
                      const char* successMessage, const char* eventDetails,
                      bool updateSensor);
  void saveRecoveryIfNeeded(uint32_t nowMs, bool force);
  void clearRecovery();
  void recordGraphSample(uint32_t nowMs);
  uint8_t ridePageCount() const;
  bool rideMediaPage() const;
  String currentClockText() const;

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
  RideRepository* repository_ = nullptr;
  PhoneLinkManager* phone_ = nullptr;
  app::AppSettings* settings_ = nullptr;
  app::AppSettings settingsEdit_;
  RainLockManager rainLock_;

  UiRouter router_;
  bool dirty_ = true;
  bool partialRainFrame_ = false;
  bool partialRideFrame_ = false;
  bool partialDynamicFrame_ = false;
  bool wasTouched_ = false;
  bool lastPaintValid_ = false;
  int16_t lastPaintX_ = 0;
  int16_t lastPaintY_ = 0;
  uint32_t lastUiDrawMs_ = 0;
  uint32_t lastGraphSampleMs_ = 0;
  bool rideTouchActive_ = false;
  bool rideSwipeCandidate_ = false;
  bool rideSwipeHandled_ = false;
  int16_t rideTouchStartX_ = 0;
  int16_t rideTouchStartY_ = 0;
  int16_t rideTouchLastX_ = 0;
  int16_t rideTouchLastY_ = 0;
  uint32_t rideTouchStartMs_ = 0;
  bool historyTouchActive_ = false;
  int16_t historyTouchStartX_ = 0;
  int16_t historyTouchStartY_ = 0;
  int16_t historyTouchLastX_ = 0;
  int16_t historyTouchLastY_ = 0;
  uint32_t historyTouchStartMs_ = 0;

  HallSensorSnapshot sensorSnapshot_;
  RideRecoveryData pendingRecovery_;
  bool hasPendingRecovery_ = false;
  String lastMessage_;
  SdTestResult sdTestResult_;
  bool sdTestRun_ = false;
  uint8_t ridePage_ = 0;
  float graphSamples_[120] = {0.0f};
  uint8_t graphWriteIndex_ = 0;
  RideSummaryItem history_[12];
  uint8_t historyCount_ = 0;
  uint8_t historySelected_ = 0;
  uint8_t historyScrollOffset_ = 0;
  String historyMessage_;
  bool batteryLowEventLogged_ = false;
  bool batteryCriticalEventLogged_ = false;
  bool exactPreviewActive_ = false;
  uint16_t exactPreviewIndex_ = 0;
  bool settingsOpenedFromRide_ = false;
  uint32_t settingsNoticeUntilMs_ = 0;
  uint8_t lastPhoneLinkState_ = 0xFF;
  uint32_t lastPairingCode_ = 0;
  uint32_t lastMediaRevision_ = 0;
  uint32_t lastNavigationRevision_ = 0;
  uint32_t lastPhoneListRevision_ = 0;
  bool lastMediaAvailable_ = false;
  bool lastNavigationAvailable_ = false;
  uint8_t lastHeaderBatteryPercent_ = 0xFF;
  int16_t lastHeaderBatteryRemainingMinutes_ = -2;
  bool lastHeaderBatteryCharging_ = false;
  bool lastHeaderSdAvailable_ = false;
  bool headerStateInitialized_ = false;
  int64_t lastClockMinute_ = -1;
};
