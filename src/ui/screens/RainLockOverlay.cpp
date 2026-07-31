#include "ui/screens/RainLockOverlay.h"

#include <math.h>

#include "ui/UiTheme.h"
#include "ui/components/HeaderLayout.h"
#include "ui/components/IconRenderer.h"
#include "ui/components/UiComponents.h"

namespace ui {

void RainLockOverlay::draw(TFT_eSPI& tft, const RainLockManager& manager) {
  if (manager.enableToastVisible()) {
    dim(tft);
    drawEnableToast(tft);
  } else if (manager.hintVisible()) {
    drawHint(tft, manager);
  } else if (manager.overlayVisible()) {
    dim(tft);
    drawUnlock(tft, manager);
  } else if (manager.successVisible()) {
    dim(tft);
    drawSuccess(tft);
  }
}

void RainLockOverlay::drawEnableConfirm(TFT_eSPI& tft) {
  dim(tft);
  panel(tft, 60, 62, 360, 188);
  IconRenderer::draw(tft, Icon::Rain, 240, 92, WARNING);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TEXT, SURFACE_2);
  tft.drawString("Enable Rain Lock?", 240, 124, 4);
  tft.setTextColor(TEXT_MUTED, SURFACE_2);
  tft.drawString("Touch controls will be blocked.", 240, 151, 2);
  Components::button(tft, 84, 178, 146, 48, "Cancel");
  Components::button(tft, 250, 178, 146, 48, "Enable", true);
}

void RainLockOverlay::dim(TFT_eSPI& tft) {
  // RGB565 has no alpha in this renderer. Alternating black scan lines retain the
  // visible ride context while reducing its luminance without another framebuffer.
  for (int16_t y = 0; y < SCREEN_H; y += 2) {
    tft.drawFastHLine(0, y, SCREEN_W, TFT_BLACK);
  }
}

void RainLockOverlay::panel(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h) {
  tft.fillRoundRect(x, y, w, h, 16, SURFACE_2);
  tft.drawRoundRect(x, y, w, h, 16, BORDER);
}

void RainLockOverlay::drawEnableToast(TFT_eSPI& tft) {
  panel(tft, 60, 60, 360, 186);
  IconRenderer::draw(tft, Icon::Rain, 240, 94, WARNING);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TEXT, SURFACE_2);
  tft.drawString("Rain lock enabled", 240, 126, 4);
  tft.setTextColor(TEXT_MUTED, SURFACE_2);
  tft.drawString("All touches are blocked.", 240, 153, 2);
  tft.drawString("Hold both points for 2 seconds to begin.", 240, 176, 1);
  const int16_t targets[] = {RainLockManager::kLeftX, RainLockManager::kRightX};
  for (int16_t x : targets) {
    tft.drawCircle(x, RainLockManager::kTargetY + 14, 18, ACCENT);
    tft.fillCircle(x, RainLockManager::kTargetY + 14, 5, ACCENT);
  }
}

void RainLockOverlay::drawHint(TFT_eSPI& tft,
                               const RainLockManager& manager) {
  constexpr int16_t x = 162;
  constexpr int16_t y = 4;
  constexpr int16_t w = 250;
  constexpr int16_t h = 32;
  tft.fillRoundRect(x, y, w, h, 9, SURFACE_2);
  tft.drawRoundRect(x, y, w, h, 9, WARNING);
  tft.fillTriangle(x + 1, y + 10, headerRainIconCenterX() + 14,
                   y + h / 2, x + 1, y + h - 10, WARNING);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TEXT, SURFACE_2);
  tft.drawString(manager.state() == RainLockState::Priming
                     ? "Keep holding both points"
                     : "Hold 2 points to unlock",
                 x + w / 2 + 3, y + h / 2, 1);
  const int16_t targets[] = {RainLockManager::kLeftX,
                             RainLockManager::kRightX};
  const uint16_t targetColor =
      manager.state() == RainLockState::Priming ? ACCENT : TEXT_MUTED;
  for (int16_t targetX : targets) {
    tft.drawCircle(targetX, RainLockManager::kTargetY, 10, ACCENT_DIM);
    tft.fillCircle(targetX, RainLockManager::kTargetY, 4, targetColor);
  }
}

void RainLockOverlay::drawUnlock(TFT_eSPI& tft, const RainLockManager& manager) {
  panel(tft, 60, 60, 360, 186);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TEXT, SURFACE_2);
  tft.drawString("Hold both points", 240, 98, 4);
  char progressText[20];
  snprintf(progressText, sizeof(progressText), "%.1f / 3.0 s",
           manager.holdElapsedMs() / 1000.0f);
  tft.setTextColor(ACCENT, SURFACE_2);
  tft.drawString(progressText, 240, 122, 2);

  drawRipples(tft, RainLockManager::kLeftX, RainLockManager::kTargetY, manager.nowMs());
  drawRipples(tft, RainLockManager::kRightX, RainLockManager::kTargetY, manager.nowMs());
  tft.fillRoundRect(110, 232, 260, 6, 3, SURFACE);
  const int16_t fill = static_cast<int16_t>(260.0f * manager.progress());
  if (fill > 0) tft.fillRoundRect(110, 232, fill, 6, 3, ACCENT);
}

void RainLockOverlay::drawSuccess(TFT_eSPI& tft) {
  panel(tft, 110, 92, 260, 128);
  IconRenderer::draw(tft, Icon::Check, 240, 128, SUCCESS);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TEXT, SURFACE_2);
  tft.drawString("Rain lock disabled", 240, 168, 2);
  tft.setTextColor(TEXT_MUTED, SURFACE_2);
  tft.drawString("Touch input restored", 240, 193, 1);
}

void RainLockOverlay::drawRipples(TFT_eSPI& tft, int16_t cx, int16_t cy, uint32_t nowMs) {
  constexpr float periodMs = 800.0f;
  constexpr float offsets[] = {0.0f, 0.33f, 0.66f};
  for (uint8_t i = 0; i < 3; ++i) {
    float phase = fmodf((nowMs % 800) / periodMs + offsets[i], 1.0f);
    const int16_t radius = 12 + static_cast<int16_t>(phase * 40.0f);
    const uint16_t color = phase < 0.34f ? ACCENT : ACCENT_DIM;
    tft.drawCircle(cx, cy, radius, color);
    if (phase < 0.45f) tft.drawCircle(cx, cy, radius + 1, color);
  }
  tft.drawCircle(cx, cy, 18, ACCENT);
  tft.fillCircle(cx, cy, 6, ACCENT);
  tft.fillCircle(cx, cy, 2, BG);
}

}  // namespace ui
