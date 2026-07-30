#include "ui/components/UiComponents.h"

#include "ui/UiTheme.h"

namespace ui {

namespace {

int16_t gBatteryRemainingMinutes = -1;
bool gBatteryCharging = false;

}  // namespace

void Components::setBatteryRuntimeEstimate(int16_t remainingMinutes,
                                           bool charging) {
  gBatteryRemainingMinutes = remainingMinutes;
  gBatteryCharging = charging;
}

void Components::header(TFT_eSPI& tft, const String& title, const HeaderStatus& status) {
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, BG);
  tft.drawFastHLine(12, 39, 456, BORDER);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TEXT, BG);
  if (status.showBack) {
    tft.drawString("<", 16, 20, 2);
    tft.drawString(title, 45, 20, 2);
  } else {
    tft.setTextColor(TEXT_MUTED, BG);
    tft.drawString(title.length() ? title : "--:--", 16, 20, 2);
  }

  int16_t right = 458;
  if (status.batteryAvailable) {
    const int16_t x = right - 29;
    tft.drawRoundRect(x, 13, 26, 14, 2, TEXT_MUTED);
    tft.fillRect(x + 26, 17, 3, 6, TEXT_MUTED);
    const int16_t fill = static_cast<int16_t>(
        (constrain(status.batteryPercent, 0, 100) * 20) / 100);
    if (fill > 0) {
      tft.fillRect(x + 3, 16, fill, 8,
                   status.batteryPercent <= 15 ? DANGER : SUCCESS);
    }
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(TEXT, BG);
    tft.drawString(String(status.batteryPercent) + "%", x - 5, 20, 1);
    char remaining[14] = "~ --";
    if (gBatteryCharging) {
      strlcpy(remaining, "CHG", sizeof(remaining));
    } else if (gBatteryRemainingMinutes >= 0) {
      const uint16_t hours = gBatteryRemainingMinutes / 60;
      const uint8_t minutes = gBatteryRemainingMinutes % 60;
      snprintf(remaining, sizeof(remaining), "~ %uh %02um",
               static_cast<unsigned>(hours),
               static_cast<unsigned>(minutes));
    }
    tft.setTextColor(TEXT_MUTED, BG);
    tft.drawString(remaining, x - 39, 20, 1);
    right -= 128;
  } else {
    IconRenderer::draw(tft, Icon::Battery, right - 17, 20, TEXT_MUTED);
    right -= 42;
  }

  tft.drawRect(right - 18, 13, 19, 14, status.sdAvailable ? TEXT_MUTED : DANGER);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(status.sdAvailable ? TEXT_MUTED : DANGER, BG);
  tft.drawString("SD", right - 8, 20, 1);
  right -= 34;

  if (status.phoneConnected) {
    IconRenderer::draw(tft, Icon::Phone, right - 7, 20, ACCENT);
  }
  right -= 34;

  if (status.showRain) {
    IconRenderer::draw(tft, Icon::Rain, right - 7, 20,
                       status.rainLocked ? ORANGE : TEXT_MUTED);
  }
  if (status.showSettings) {
    IconRenderer::draw(tft, Icon::Settings, 82, 20, TEXT_MUTED);
  }
}

void Components::button(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h,
                        const String& label, bool primary, bool danger, bool enabled) {
  const uint16_t fill = !enabled ? SURFACE : (primary ? LABEL : SURFACE);
  const uint16_t border = !enabled ? BORDER : (danger ? DANGER : (primary ? LABEL : BORDER));
  const uint16_t text = !enabled ? TEXT_MUTED : (danger ? DANGER : (primary ? BG : TEXT));
  tft.fillRoundRect(x, y, w, h, 10, fill);
  tft.drawRoundRect(x, y, w, h, 10, border);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(text, fill);
  tft.drawString(label, x + w / 2, y + h / 2, 2);
}

void Components::card(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h,
                      const String& label, const String& value, const String& unit, bool accent) {
  tft.fillRoundRect(x, y, w, h, 12, SURFACE);
  tft.drawRoundRect(x, y, w, h, 12, BORDER);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TEXT_MUTED, SURFACE);
  tft.drawString(label, x + 12, y + 12, 1);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(accent ? LABEL : TEXT, SURFACE);
  tft.drawString(value, x + w / 2, y + h / 2 + 5,
                 value.length() > 6 ? 2 : 4);
  if (unit.length()) {
    tft.setTextDatum(BR_DATUM);
    tft.setTextColor(TEXT_MUTED, SURFACE);
    tft.drawString(unit, x + w - 9, y + h - 12, 1);
  }
}

void Components::menuTile(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, Icon icon,
                          const String& title, const String& subtitle, bool active,
                          bool enabled, uint8_t textInset) {
  const uint16_t fill = SURFACE;
  const uint16_t border = active && enabled ? ACCENT : BORDER;
  const uint16_t fg = enabled ? (active ? ACCENT : TEXT) : TEXT_MUTED;
  tft.fillRoundRect(x, y, w, h, 14, fill);
  tft.drawRoundRect(x, y, w, h, 14, border);
  if (active && enabled) {
    tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 13, border);
  }
  IconRenderer::draw(tft, icon, x + 33, y + h / 2, fg);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(fg, fill);
  tft.drawString(title, x + textInset,
                 y + (subtitle.length() ? 14 : 20), 2);
  if (subtitle.length()) {
    tft.setTextColor(TEXT_MUTED, fill);
    tft.drawString(subtitle, x + textInset, y + 36, 1);
  }
}

void Components::stateChip(TFT_eSPI& tft, const String& label, uint16_t color) {
  tft.fillCircle(21, 55, 3, color);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TEXT_MUTED, BG);
  tft.drawString(label, 29, 55, 1);
}

void Components::pageDots(TFT_eSPI& tft, uint8_t active, uint8_t count, int16_t y) {
  if (count < 2) return;
  const int16_t spacing = 16;
  const int16_t start =
      (SCREEN_W - (count * 8 + (count - 1) * 8)) / 2;
  for (uint8_t i = 0; i < count; ++i) {
    if (i == active) {
      tft.fillRoundRect(start + i * spacing, y - 3, 8, 6, 3, ACCENT);
    } else {
      tft.fillCircle(start + i * spacing + 3, y, 2, TEXT_MUTED);
    }
  }
}

}  // namespace ui
