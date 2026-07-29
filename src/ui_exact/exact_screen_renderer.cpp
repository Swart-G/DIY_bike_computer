#include "ui_exact/exact_screen_renderer.h"

namespace ui_exact {

void ExactScreenRenderer::draw(ScreenId id) {
  draw(getScreenAsset(id));
}

void ExactScreenRenderer::draw(const ScreenAsset& asset) {
  uint32_t emittedPixels = 0;
  uint32_t sourcePixel = 0;
  for (uint32_t i = 0; i < asset.runCount; ++i) {
    const uint32_t packed = pgm_read_dword(&asset.runs[i]);
    uint32_t remaining = packed >> 16;
    const uint16_t color = static_cast<uint16_t>(packed & 0xFFFFu);
    while (remaining > 0) {
      const uint16_t sourceY = sourcePixel / kScreenWidth;
      const uint16_t sourceX = sourcePixel % kScreenWidth;
      const uint16_t rowPixels =
          min<uint32_t>(remaining, kScreenWidth - sourceX);

      // drawFastHLine is virtual in TFT_eSPI. Unlike the non-virtual
      // pushColor(color, count) overload, it dispatches correctly when tft_ is
      // the active TFT_eSprite framebuffer.
      tft_.drawFastHLine(sourceX, sourceY, rowPixels, color);
      sourcePixel += rowPixels;
      remaining -= rowPixels;
      emittedPixels += rowPixels;
    }
  }

  // A valid asset always emits exactly 480 * 320 pixels.
  // Keep the variable for debugger inspection without adding Serial noise.
  (void)emittedPixels;
}

void ExactScreenRenderer::drawRegion(ScreenId id, uint16_t x, uint16_t y,
                                     uint16_t w, uint16_t h) {
  drawRegion(getScreenAsset(id), x, y, w, h);
}

void ExactScreenRenderer::drawRegion(const ScreenAsset& asset, uint16_t x,
                                     uint16_t y, uint16_t w, uint16_t h) {
  if (x >= kScreenWidth || y >= kScreenHeight || w == 0 || h == 0) {
    return;
  }
  if (x + w > kScreenWidth) w = kScreenWidth - x;
  if (y + h > kScreenHeight) h = kScreenHeight - y;

  uint32_t sourcePixel = 0;
  uint32_t emittedPixels = 0;
  for (uint32_t i = 0; i < asset.runCount; ++i) {
    const uint32_t packed = pgm_read_dword(&asset.runs[i]);
    uint32_t remaining = packed >> 16;
    const uint16_t color = static_cast<uint16_t>(packed & 0xFFFFu);
    while (remaining > 0) {
      const uint16_t sourceY = sourcePixel / kScreenWidth;
      const uint16_t sourceX = sourcePixel % kScreenWidth;
      const uint16_t rowPixels =
          min<uint32_t>(remaining, kScreenWidth - sourceX);
      if (sourceY >= y && sourceY < y + h) {
        const uint16_t segmentEnd = sourceX + rowPixels;
        const uint16_t visibleStart = max<uint16_t>(sourceX, x);
        const uint16_t visibleEnd = min<uint16_t>(segmentEnd, x + w);
        if (visibleEnd > visibleStart) {
          const uint16_t visiblePixels = visibleEnd - visibleStart;
          tft_.drawFastHLine(visibleStart, sourceY, visiblePixels, color);
          emittedPixels += visiblePixels;
        }
      }
      sourcePixel += rowPixels;
      remaining -= rowPixels;
    }
  }

  (void)emittedPixels;
}

} // namespace ui_exact
