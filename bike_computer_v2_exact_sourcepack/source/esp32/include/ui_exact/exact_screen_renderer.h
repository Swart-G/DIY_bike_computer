#pragma once
#include <TFT_eSPI.h>
#include "screen_assets.h"

namespace ui_exact {

class ExactScreenRenderer {
public:
  explicit ExactScreenRenderer(TFT_eSPI& tft) : tft_(tft) {}
  void draw(ScreenId id);
  void draw(const ScreenAsset& asset);

private:
  TFT_eSPI& tft_;
};

} // namespace ui_exact
