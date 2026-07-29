#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "navigation/NavigationState.h"

namespace ui {

enum class Icon : uint8_t {
  Bike,
  History,
  Phone,
  Settings,
  Display,
  Diagnostics,
  Storage,
  System,
  Info,
  Battery,
  Usb,
  Rain,
  Check,
  MediaPrevious,
  MediaPlay,
  MediaPause,
  MediaNext,
};

class IconRenderer {
 public:
  static void draw(TFT_eSPI& tft, Icon icon, int16_t cx, int16_t cy, uint16_t color,
                   uint8_t scale = 1);
  static void drawManeuver(TFT_eSPI& tft, navigation::Maneuver maneuver,
                           int16_t cx, int16_t cy, uint16_t color);

 private:
  static void line2(TFT_eSPI& tft, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    uint16_t color);
};

}  // namespace ui
