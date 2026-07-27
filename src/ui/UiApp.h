#pragma once

#include <Arduino.h>

#include "battery/BatteryMonitor.h"
#include "display/DisplayManager.h"
#include "speed/HallSensor.h"
#include "speed/RideStateMachine.h"
#include "speed/SpeedCalculator.h"
#include "storage/StorageManager.h"
#include "touch/TouchManager.h"
#include "usb/UsbMassStorageManager.h"

class UiApp {
 public:
  void begin(DisplayManager& display, TouchManager& touch, StorageManager& storage,
             UsbMassStorageManager& usb, HallSensor& sensor, SpeedCalculator& speed,
             RideStateMachine& ride, BatteryMonitor& battery, app::AppSettings& settings);
  void loop();

 private:
  enum class Screen {
    SdMissing,
    Recovery,
    MainMenu,
    Diagnostics,
    DisplayTest,
    TouchRawTest,
    PaintTest,
    SdTest,
    UsbStorage,
    SensorTest,
    BatteryTest,
    SystemInfo,
    Settings,
    About,
    Ride,
  };

  void enter(Screen screen);
  void updateModel(uint32_t nowMs);
  void handleTap(int16_t x, int16_t y);
  void handleRideTouchStart(int16_t x, int16_t y, uint32_t nowMs);
  void handleRideTouchMove(int16_t x, int16_t y);
  void handleRideTouchEnd(uint32_t nowMs);
  void handlePaint();
  void draw();

  void drawSdMissing();
  void drawRecovery();
  void drawMainMenu();
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
  void drawAbout();
  void drawRide();

  void drawStatusBar(const String& title, const String& status = String());
  void drawStorageStatusIcon(int16_t x, int16_t y);
  void drawBatteryStatusIcon(int16_t x, int16_t y);
  void drawBottomRideControls();
  void drawPageDots(uint8_t activePage, uint8_t pageCount);
  void drawMetricCard(int16_t x, int16_t y, int16_t w, int16_t h, const String& label,
                      const String& value, const String& unit = String());
  void drawMenuTile(int16_t x, int16_t y, int16_t w, int16_t h, const String& title, bool selected,
                    bool enabled);
  void drawGraphCard(int16_t x, int16_t y, int16_t w, int16_t h);
  void drawSoftButton(int16_t x, int16_t y, int16_t w, int16_t h, const String& label,
                      uint16_t accentColor, bool enabled = true, uint16_t fillColor = 0xFFFF);
  void drawBackButton();
  void drawTextBlock(const String& text, int16_t x, int16_t y, int16_t lineHeight,
                     uint16_t color = TFT_WHITE, uint16_t bg = TFT_BLACK, uint8_t font = 2);
  bool hit(int16_t x, int16_t y, int16_t bx, int16_t by, int16_t bw, int16_t bh) const;
  String durationText(uint32_t ms) const;
  String storageStatusShort() const;
  String batteryStatusShort() const;
  String rideStatusLine() const;
  void startUsbMode();
  void saveRecoveryIfNeeded(uint32_t nowMs, bool force);
  void clearRecovery();
  void recordGraphSample(uint32_t nowMs);

  DisplayManager* display_ = nullptr;
  TouchManager* touch_ = nullptr;
  StorageManager* storage_ = nullptr;
  UsbMassStorageManager* usb_ = nullptr;
  HallSensor* sensor_ = nullptr;
  SpeedCalculator* speed_ = nullptr;
  RideStateMachine* ride_ = nullptr;
  BatteryMonitor* battery_ = nullptr;
  app::AppSettings* settings_ = nullptr;

  Screen screen_ = Screen::MainMenu;
  bool dirty_ = true;
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

  HallSensorSnapshot sensorSnapshot_;
  RideRecoveryData pendingRecovery_;
  bool hasPendingRecovery_ = false;
  String lastMessage_;
  SdTestResult sdTestResult_;
  bool sdTestRun_ = false;
  uint8_t ridePage_ = 0;
  float graphSamples_[120] = {0.0f};
  uint8_t graphWriteIndex_ = 0;
};
