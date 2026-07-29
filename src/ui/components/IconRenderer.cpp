#include "ui/components/IconRenderer.h"

#include "ui/UiTheme.h"

namespace ui {

void IconRenderer::line2(TFT_eSPI& tft, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                         uint16_t color) {
  tft.drawLine(x0, y0, x1, y1, color);
  if (abs(x1 - x0) > abs(y1 - y0)) {
    tft.drawLine(x0, y0 + 1, x1, y1 + 1, color);
  } else {
    tft.drawLine(x0 + 1, y0, x1 + 1, y1, color);
  }
}

void IconRenderer::draw(TFT_eSPI& tft, Icon icon, int16_t cx, int16_t cy, uint16_t color,
                        uint8_t scale) {
  const int16_t s = max<uint8_t>(1, scale);
  switch (icon) {
    case Icon::Bike: {
      const int16_t r = 7 * s;
      const int16_t lx = cx - 20 * s;
      const int16_t rx = cx + 20 * s;
      tft.drawCircle(lx, cy + 7 * s, r, color);
      tft.drawCircle(rx, cy + 7 * s, r, color);
      line2(tft, lx, cy + 7 * s, cx - 6 * s, cy - 7 * s, color);
      line2(tft, cx - 6 * s, cy - 7 * s, cx + 7 * s, cy + 7 * s, color);
      line2(tft, lx, cy + 7 * s, cx + 7 * s, cy + 7 * s, color);
      line2(tft, cx - 6 * s, cy - 7 * s, cx + 12 * s, cy - 7 * s, color);
      line2(tft, cx + 12 * s, cy - 7 * s, rx, cy + 7 * s, color);
      break;
    }
    case Icon::History:
      tft.drawCircle(cx, cy, 13 * s, color);
      line2(tft, cx, cy, cx, cy - 8 * s, color);
      line2(tft, cx, cy, cx + 7 * s, cy + 4 * s, color);
      line2(tft, cx - 13 * s, cy, cx - 18 * s, cy - 5 * s, color);
      break;
    case Icon::Phone:
      tft.drawRoundRect(cx - 7 * s, cy - 13 * s, 14 * s, 26 * s, 2 * s, color);
      tft.fillCircle(cx, cy + 9 * s, s, color);
      break;
    case Icon::Settings:
      tft.drawCircle(cx, cy, 9 * s, color);
      tft.drawCircle(cx, cy, 3 * s, color);
      tft.drawFastVLine(cx, cy - 15 * s, 6 * s, color);
      tft.drawFastVLine(cx, cy + 10 * s, 6 * s, color);
      tft.drawFastHLine(cx - 15 * s, cy, 6 * s, color);
      tft.drawFastHLine(cx + 10 * s, cy, 6 * s, color);
      break;
    case Icon::Display:
      tft.drawRoundRect(cx - 15 * s, cy - 11 * s, 30 * s, 21 * s, 2 * s, color);
      tft.drawFastHLine(cx - 6 * s, cy + 14 * s, 12 * s, color);
      tft.drawFastVLine(cx, cy + 10 * s, 5 * s, color);
      break;
    case Icon::Diagnostics:
      line2(tft, cx - 16 * s, cy, cx - 9 * s, cy, color);
      line2(tft, cx - 9 * s, cy, cx - 4 * s, cy - 10 * s, color);
      line2(tft, cx - 4 * s, cy - 10 * s, cx + 3 * s, cy + 10 * s, color);
      line2(tft, cx + 3 * s, cy + 10 * s, cx + 9 * s, cy - 3 * s, color);
      line2(tft, cx + 9 * s, cy - 3 * s, cx + 16 * s, cy - 3 * s, color);
      break;
    case Icon::Storage:
      tft.drawRoundRect(cx - 14 * s, cy - 11 * s, 28 * s, 22 * s, 3 * s, color);
      tft.drawFastHLine(cx - 8 * s, cy - 3 * s, 16 * s, color);
      tft.fillCircle(cx + 8 * s, cy + 5 * s, 2 * s, color);
      break;
    case Icon::System:
      tft.drawRect(cx - 12 * s, cy - 12 * s, 24 * s, 24 * s, color);
      for (int8_t i = -8; i <= 8; i += 8) {
        tft.drawFastVLine(cx + i * s, cy - 16 * s, 4 * s, color);
        tft.drawFastVLine(cx + i * s, cy + 13 * s, 4 * s, color);
      }
      break;
    case Icon::Info:
      tft.drawCircle(cx, cy, 14 * s, color);
      tft.fillCircle(cx, cy - 7 * s, 2 * s, color);
      tft.fillRoundRect(cx - 2 * s, cy - 2 * s, 4 * s, 11 * s, s, color);
      break;
    case Icon::Battery:
      tft.drawRoundRect(cx - 15 * s, cy - 7 * s, 27 * s, 14 * s, 2 * s, color);
      tft.fillRect(cx + 13 * s, cy - 3 * s, 3 * s, 6 * s, color);
      break;
    case Icon::Usb:
      line2(tft, cx, cy - 15 * s, cx, cy + 12 * s, color);
      line2(tft, cx, cy - 3 * s, cx - 10 * s, cy + 4 * s, color);
      line2(tft, cx, cy, cx + 10 * s, cy - 7 * s, color);
      tft.fillCircle(cx, cy + 14 * s, 2 * s, color);
      tft.fillTriangle(cx - 4 * s, cy - 15 * s, cx + 4 * s, cy - 15 * s, cx, cy - 21 * s,
                       color);
      break;
    case Icon::Rain:
      tft.drawCircle(cx, cy + 3 * s, 7 * s, color);
      tft.fillTriangle(cx - 6 * s, cy, cx + 6 * s, cy, cx, cy - 10 * s, color);
      break;
    case Icon::Check:
      tft.drawCircle(cx, cy, 14 * s, color);
      line2(tft, cx - 7 * s, cy, cx - 2 * s, cy + 6 * s, color);
      line2(tft, cx - 2 * s, cy + 6 * s, cx + 8 * s, cy - 7 * s, color);
      break;
    case Icon::MediaPrevious:
      tft.fillTriangle(cx - 10 * s, cy, cx + 4 * s, cy - 10 * s,
                       cx + 4 * s, cy + 10 * s, color);
      tft.drawFastVLine(cx - 12 * s, cy - 10 * s, 21 * s, color);
      break;
    case Icon::MediaPlay:
      tft.fillTriangle(cx - 7 * s, cy - 11 * s, cx - 7 * s, cy + 11 * s,
                       cx + 11 * s, cy, color);
      break;
    case Icon::MediaPause:
      tft.fillRect(cx - 9 * s, cy - 11 * s, 6 * s, 22 * s, color);
      tft.fillRect(cx + 3 * s, cy - 11 * s, 6 * s, 22 * s, color);
      break;
    case Icon::MediaNext:
      tft.fillTriangle(cx + 10 * s, cy, cx - 4 * s, cy - 10 * s,
                       cx - 4 * s, cy + 10 * s, color);
      tft.drawFastVLine(cx + 12 * s, cy - 10 * s, 21 * s, color);
      break;
  }
}

void IconRenderer::drawManeuver(TFT_eSPI& tft,
                                navigation::Maneuver maneuver, int16_t cx,
                                int16_t cy, uint16_t color) {
  const bool left =
      maneuver == navigation::Maneuver::TurnLeft ||
      maneuver == navigation::Maneuver::SlightLeft ||
      maneuver == navigation::Maneuver::SharpLeft;
  const bool right =
      maneuver == navigation::Maneuver::TurnRight ||
      maneuver == navigation::Maneuver::SlightRight ||
      maneuver == navigation::Maneuver::SharpRight;
  if (left || right) {
    const int16_t direction = left ? -1 : 1;
    const int16_t turnX = cx + direction * 19;
    line2(tft, cx - direction * 18, cy + 17, cx - direction * 18, cy,
          color);
    line2(tft, cx - direction * 18, cy, turnX, cy, color);
    tft.fillTriangle(turnX + direction * 13, cy, turnX, cy - 11,
                     turnX, cy + 11, color);
    return;
  }
  if (maneuver == navigation::Maneuver::Uturn) {
    tft.drawCircle(cx, cy, 18, color);
    tft.fillRect(cx - 20, cy, 41, 22, ui::BG);
    line2(tft, cx - 18, cy, cx - 18, cy + 18, color);
    tft.fillTriangle(cx - 18, cy - 8, cx - 28, cy + 3, cx - 8, cy + 3,
                     color);
    return;
  }
  if (maneuver == navigation::Maneuver::Roundabout ||
      maneuver == navigation::Maneuver::RoundaboutExit) {
    tft.drawCircle(cx, cy, 18, color);
    tft.fillTriangle(cx + 18, cy - 7, cx + 28, cy - 2, cx + 18, cy + 4,
                     color);
    return;
  }
  if (maneuver == navigation::Maneuver::Destination) {
    tft.drawCircle(cx, cy - 5, 11, color);
    tft.fillTriangle(cx - 8, cy + 1, cx + 8, cy + 1, cx, cy + 20, color);
    tft.fillCircle(cx, cy - 5, 3, color);
    return;
  }
  line2(tft, cx, cy + 20, cx, cy - 15, color);
  tft.fillTriangle(cx, cy - 25, cx - 11, cy - 11, cx + 11, cy - 11,
                   color);
}

}  // namespace ui
