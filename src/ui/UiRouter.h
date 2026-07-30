#pragma once

#include <Arduino.h>

enum class UiScreen : uint8_t {
  SdMissing,
  Recovery,
  Home,
  Phone,
  History,
  HistoryDetail,
  DeleteRideConfirm,
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
  SettingsRide,
  SettingsDisplay,
  SettingsSystem,
  SettingsWheel,
  SettingsStopThreshold,
  SettingsAutoPauseDelay,
  SettingsLogInterval,
  SettingsRgbLed,
  SettingsRgbStableRange,
  SettingsRgbStableRange5s,
  SettingsRgbStableRange10s,
  SettingsRgbBrightness,
  Ride,
  FinishConfirm,
  RideSummary,
};

class UiRouter {
 public:
  UiScreen current() const { return current_; }
  UiScreen previous() const { return previous_; }

  void go(UiScreen screen) {
    if (screen == current_) return;
    previous_ = current_;
    current_ = screen;
  }

 private:
  UiScreen current_ = UiScreen::Home;
  UiScreen previous_ = UiScreen::Home;
};
