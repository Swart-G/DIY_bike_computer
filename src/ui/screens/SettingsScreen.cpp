#include "ui/screens/SettingsScreen.h"

#include "ui/UiTheme.h"
#include "ui_exact/exact_screen_renderer.h"

namespace ui {

void SettingsScreen::row(TFT_eSPI& tft, int16_t y, const String& label,
                         const String& value, bool interactive) {
  tft.fillRoundRect(18, y, 444, 48, 10, SURFACE);
  tft.drawRoundRect(18, y, 444, 48, 10, BORDER);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TEXT, SURFACE);
  tft.drawString(label, 32, y + 24, 2);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(TEXT_MUTED, SURFACE);
  tft.drawString(value, interactive ? 448 : 456, y + 24, 1);
  if (interactive) tft.drawString(">", 458, y + 24, 2);
}

void SettingsScreen::toggleRow(TFT_eSPI& tft, int16_t y,
                               const String& label, bool enabled,
                               const String& detail,
                               bool detailInteractive) {
  tft.fillRoundRect(18, y, 444, 48, 10, SURFACE);
  tft.drawRoundRect(18, y, 444, 48, 10, BORDER);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TEXT, SURFACE);
  tft.drawString(label, 32, y + 24, 2);
  if (detail.length()) {
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(TEXT_MUTED, SURFACE);
    tft.drawString(detail, detailInteractive ? 370 : 388, y + 24, 1);
    if (detailInteractive) {
      tft.drawString(">", 389, y + 24, 2);
    }
  }

  const int16_t switchX = 398;
  const int16_t switchY = y + 12;
  const uint16_t switchFill = enabled ? ACCENT : SURFACE_2;
  const uint16_t switchBorder = enabled ? ACCENT : BORDER;
  tft.fillRoundRect(switchX, switchY, 50, 24, 12, switchFill);
  tft.drawRoundRect(switchX, switchY, 50, 24, 12, switchBorder);
  tft.fillCircle(enabled ? switchX + 38 : switchX + 12,
                 switchY + 12, 9, enabled ? BG : TEXT_MUTED);
}

void SettingsScreen::valueEditor(
    TFT_eSPI& tft, const SettingsStatus& status, const String& title,
    const String& value, const String& unit, float position, float minimum,
    float maximum, const String& minimumText, const String& maximumText) {
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_75_SETTINGS_WHEEL);
  Components::header(tft, title, status.header);
  tft.fillRect(120, 76, 240, 78, BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TEXT, BG);
  tft.drawString(value, 240, 104, 6);
  tft.setTextColor(TEXT_MUTED, BG);
  tft.drawString(unit, 240, 137, 2);
  tft.fillRect(92, 243, 296, 11, BG);
  tft.drawWideLine(94, 248, 386, 248, 4.0f, BORDER, BG);
  const float ratio =
      constrain((position - minimum) / (maximum - minimum), 0.0f, 1.0f);
  const int16_t marker = 94 + static_cast<int16_t>(ratio * 292.0f);
  tft.drawWideLine(94, 248, marker, 248, 4.0f, ACCENT, BG);
  tft.fillCircle(marker, 248, 5, ACCENT);
  Components::button(tft, 74, 171, 86, 52, "-");
  Components::button(tft, 320, 171, 86, 52, "+", true);
  tft.fillRect(88, 258, 304, 24, BG);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TEXT_MUTED, BG);
  tft.drawString(minimumText, 94, 269, 1);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(maximumText, 386, 269, 1);
  Components::button(tft, 150, 287, 180, 27, "Save", true);
}

void SettingsScreen::drawMain(TFT_eSPI& tft, const SettingsStatus& status) {
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_70_SETTINGS_MAIN);
  Components::header(tft, "Settings", status.header);
  // The exact assets remain the geometry baseline. Redraw labels and icons
  // with native TFT fonts so small text stays crisp on the physical panel.
  Components::menuTile(tft, 18, 57, 214, 62, Icon::Bike, "Ride",
                       "Wheel & speed", false, !status.rideActive, 70);
  Components::menuTile(tft, 248, 57, 214, 62, Icon::Display, "Display",
                       "Theme & layout");
  Components::menuTile(tft, 18, 131, 214, 62, Icon::Phone, "Phone",
                       "Pairing & sync");
  Components::menuTile(tft, 248, 131, 214, 62, Icon::System, "System",
                       "Storage & device");
  Components::menuTile(tft, 18, 205, 214, 62, Icon::Diagnostics,
                       "Diagnostics", "Hardware tests", false,
                       !status.rideActive);
  Components::menuTile(tft, 248, 205, 214, 62, Icon::Info, "Speed LED",
                       "2 s speed trend", false, !status.rideActive);
  if (status.showRideLockNotice) {
    tft.fillRoundRect(69, 276, 342, 32, 10, WARNING);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(BG, WARNING);
    tft.drawString("Cannot change settings during a ride", 240, 292, 2);
  }
}

void SettingsScreen::drawRide(TFT_eSPI& tft, const SettingsStatus& status,
                              const app::AppSettings& settings) {
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_71_SETTINGS_RIDE);
  Components::header(tft, "Ride settings", status.header);
  row(tft, 57, "Wheel circumference", String(settings.wheelCircumferenceM, 3) + " m");
  row(tft, 113, "Stop threshold", String(settings.stopThresholdKmh, 1) + " km/h");
  toggleRow(tft, 169, "Auto pause", settings.autoPauseEnabled,
            String(settings.autoPauseDelayMs / 1000.0f, 1) + " s", true);
  row(tft, 225, "Log interval",
      String(settings.logSampleIntervalMs / 1000.0f, 2) + " s");
}

void SettingsScreen::drawDisplay(TFT_eSPI& tft, const SettingsStatus& status) {
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_72_SETTINGS_DISPLAY);
  Components::header(tft, "Display", status.header);
  row(tft, 57, "Theme", "Dark", false);
  row(tft, 113, "Screen timeout", "Never", false);
  row(tft, 169, "Ride page order", "Speed / Stats / Graph", false);
  row(tft, 225, "Status chips", "Compact", false);
}

void SettingsScreen::drawSystem(TFT_eSPI& tft, const SettingsStatus& status) {
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_74_SETTINGS_SYSTEM);
  Components::header(tft, "System", status.header);
  row(tft, 57, "SD card", status.sdText, false);
  row(tft, 113, "USB storage",
      status.rideActive
          ? "Locked during ride"
          : (status.usbActive ? "Active" : "Inactive"));
  row(tft, 169, "Time source", status.timeSource, false);
  row(tft, 225, "Firmware", app::FIRMWARE_VERSION, false);
  if (status.showRideLockNotice) {
    tft.fillRoundRect(69, 276, 342, 32, 10, WARNING);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(BG, WARNING);
    tft.drawString("Cannot change settings during a ride", 240, 292, 2);
  }
}

void SettingsScreen::drawWheel(TFT_eSPI& tft, const SettingsStatus& status,
                               const app::AppSettings& settings) {
  valueEditor(tft, status, "Wheel circumference",
              String(settings.wheelCircumferenceM, 3), "m",
              settings.wheelCircumferenceM, 0.5f, 3.5f, "0.500", "3.500");
}

void SettingsScreen::drawStopThreshold(TFT_eSPI& tft, const SettingsStatus& status,
                                       const app::AppSettings& settings) {
  valueEditor(tft, status, "Stop threshold",
              String(settings.stopThresholdKmh, 1), "km/h",
              settings.stopThresholdKmh, 0.5f, 15.0f, "0.5", "15.0");
}

void SettingsScreen::drawLogInterval(TFT_eSPI& tft,
                                     const SettingsStatus& status,
                                     const app::AppSettings& settings) {
  const float seconds = settings.logSampleIntervalMs / 1000.0f;
  valueEditor(tft, status, "Log interval", String(seconds, 2), "s",
              seconds, 0.25f, 10.0f, "0.25", "10.00");
}

void SettingsScreen::drawAutoPauseDelay(
    TFT_eSPI& tft, const SettingsStatus& status,
    const app::AppSettings& settings) {
  const float seconds = settings.autoPauseDelayMs / 1000.0f;
  valueEditor(tft, status, "Auto pause delay", String(seconds, 1), "s",
              seconds, 1.0f, 60.0f, "1.0", "60.0");
}

void SettingsScreen::drawRgbLed(TFT_eSPI& tft,
                                const SettingsStatus& status,
                                const app::AppSettings& settings) {
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_72_SETTINGS_DISPLAY);
  Components::header(tft, "Speed LED", status.header);
  toggleRow(tft, 57, "Indicator", settings.rgbSpeedTrendEnabled);
  row(tft, 113, "Stable range",
      "+/- " + String(settings.rgbSpeedTrendToleranceKmh, 1) + " km/h");
  row(tft, 169, "Brightness",
      String(settings.rgbLedBrightnessPercent) + "%");
  row(tft, 225, "Comparison window", "2.0 s", false);

  tft.fillRect(18, 279, 444, 31, BG);
  tft.setTextDatum(ML_DATUM);
  tft.fillCircle(31, 294, 4, PURPLE);
  tft.setTextColor(TEXT_MUTED, BG);
  tft.drawString("Faster", 40, 294, 1);
  tft.fillCircle(171, 294, 4, SUCCESS);
  tft.drawString("Steady", 180, 294, 1);
  tft.fillCircle(311, 294, 4, DANGER);
  tft.drawString("Slower", 320, 294, 1);
}

void SettingsScreen::drawRgbStableRange(
    TFT_eSPI& tft, const SettingsStatus& status,
    const app::AppSettings& settings) {
  valueEditor(tft, status, "Stable range",
              String(settings.rgbSpeedTrendToleranceKmh, 1), "+/- km/h",
              settings.rgbSpeedTrendToleranceKmh, 0.1f, 5.0f, "0.1", "5.0");
}

void SettingsScreen::drawRgbBrightness(
    TFT_eSPI& tft, const SettingsStatus& status,
    const app::AppSettings& settings) {
  valueEditor(tft, status, "LED brightness",
              String(settings.rgbLedBrightnessPercent), "%",
              settings.rgbLedBrightnessPercent, 5.0f, 100.0f, "5", "100");
}

}  // namespace ui
