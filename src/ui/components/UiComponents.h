#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "ui/components/IconRenderer.h"

namespace ui {

struct HeaderStatus {
  bool showBack = false;
  bool showRain = false;
  bool showSettings = false;
  bool rainLocked = false;
  bool phoneConnected = false;
  bool sdAvailable = false;
  bool batteryAvailable = false;
  uint8_t batteryPercent = 0;
};

class Components {
 public:
  static void header(TFT_eSPI& tft, const String& title, const HeaderStatus& status);
  static void button(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h,
                     const String& label, bool primary = false, bool danger = false,
                     bool enabled = true);
  static void card(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h,
                   const String& label, const String& value, const String& unit = String(),
                   bool accent = false);
  static void menuTile(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, Icon icon,
                       const String& title, const String& subtitle = String(),
                       bool active = false, bool enabled = true,
                       uint8_t textInset = 62);
  static void stateChip(TFT_eSPI& tft, const String& label, uint16_t color);
  static void pageDots(TFT_eSPI& tft, uint8_t active, uint8_t count, int16_t y);
};

}  // namespace ui
