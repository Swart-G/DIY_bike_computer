#pragma once
#include <TFT_eSPI.h>
#include "screen_assets.h"

namespace ui_exact {

class ExactScreenRenderer {
public:
  explicit ExactScreenRenderer(TFT_eSPI& tft) : tft_(tft) {}
  void draw(ScreenId id);
  void draw(const ScreenAsset& asset);
  void drawRegion(ScreenId id, uint16_t x, uint16_t y, uint16_t w,
                  uint16_t h);
  void drawRegion(const ScreenAsset& asset, uint16_t x, uint16_t y,
                  uint16_t w, uint16_t h);

private:
  TFT_eSPI& tft_;
};

} // namespace ui_exact
