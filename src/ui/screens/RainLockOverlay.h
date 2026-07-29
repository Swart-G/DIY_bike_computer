#pragma once

#include <TFT_eSPI.h>

#include "rain/RainLockManager.h"

namespace ui {

class RainLockOverlay {
 public:
  static void draw(TFT_eSPI& tft, const RainLockManager& manager);

 private:
  static void dim(TFT_eSPI& tft);
  static void panel(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h);
  static void drawEnableToast(TFT_eSPI& tft);
  static void drawHint(TFT_eSPI& tft, const RainLockManager& manager);
  static void drawUnlock(TFT_eSPI& tft, const RainLockManager& manager);
  static void drawSuccess(TFT_eSPI& tft);
  static void drawRipples(TFT_eSPI& tft, int16_t cx, int16_t cy, uint32_t nowMs);
};

}  // namespace ui
