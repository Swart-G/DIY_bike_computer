#include "ui_exact/exact_screen_renderer.h"

namespace ui_exact {

void ExactScreenRenderer::draw(ScreenId id) {
  draw(getScreenAsset(id));
}

void ExactScreenRenderer::draw(const ScreenAsset& asset) {
  tft_.startWrite();
  tft_.setAddrWindow(0, 0, kScreenWidth, kScreenHeight);

  uint32_t emittedPixels = 0;
  for (uint32_t i = 0; i < asset.runCount; ++i) {
    const uint32_t packed = pgm_read_dword(&asset.runs[i]);
    const uint16_t count = static_cast<uint16_t>(packed >> 16);
    const uint16_t color = static_cast<uint16_t>(packed & 0xFFFFu);
    tft_.pushColor(color, count);
    emittedPixels += count;
  }

  tft_.endWrite();

  // A valid asset always emits exactly 480 * 320 pixels.
  // Keep the variable for debugger inspection without adding Serial noise.
  (void)emittedPixels;
}

} // namespace ui_exact
