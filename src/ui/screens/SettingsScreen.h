#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "config/app_config.h"
#include "ui/components/UiComponents.h"

namespace ui {

struct SettingsStatus {
  HeaderStatus header;
  String sdText;
  String timeSource = "Unavailable";
  bool usbActive = false;
  bool rideActive = false;
  bool showRideLockNotice = false;
};

class SettingsScreen {
 public:
  static void drawMain(TFT_eSPI& tft, const SettingsStatus& status);
  static void drawRide(TFT_eSPI& tft, const SettingsStatus& status,
                       const app::AppSettings& settings);
  static void drawDisplay(TFT_eSPI& tft, const SettingsStatus& status);
  static void drawSystem(TFT_eSPI& tft, const SettingsStatus& status);
  static void drawWheel(TFT_eSPI& tft, const SettingsStatus& status,
                        const app::AppSettings& settings);
  static void drawStopThreshold(TFT_eSPI& tft, const SettingsStatus& status,
                                const app::AppSettings& settings);
  static void drawRgbLed(TFT_eSPI& tft, const SettingsStatus& status,
                         const app::AppSettings& settings);

 private:
  static void row(TFT_eSPI& tft, int16_t y, const String& label,
                  const String& value, bool interactive = true);
};

}  // namespace ui
