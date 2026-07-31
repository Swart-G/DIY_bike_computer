#include "ui/screens/SecondaryScreens.h"

#include "ui/UiTheme.h"
#include "ui_exact/exact_screen_renderer.h"

namespace ui {

String SecondaryScreens::duration(uint64_t ms) {
  const uint64_t seconds = ms / 1000ULL;
  char text[16];
  snprintf(text, sizeof(text), "%02llu:%02llu",
           static_cast<unsigned long long>(seconds / 60ULL),
           static_cast<unsigned long long>(seconds % 60ULL));
  return String(text);
}

void SecondaryScreens::history(TFT_eSPI& tft, const HeaderStatus& header,
                               const RideSummaryItem* rides, uint8_t count,
                               uint8_t scrollOffset,
                               const String& message) {
  ui_exact::ExactScreenRenderer exact(tft);
  exact.draw(count ? ui_exact::ScreenId::SCREEN_41_HISTORY_LIST
                   : ui_exact::ScreenId::SCREEN_40_HISTORY_EMPTY);
  Components::header(tft, "History", header);
  if (!count) {
    if (message.length()) {
      const bool failed = message.indexOf("failed") >= 0 ||
                          message.indexOf("Cannot") >= 0;
      tft.fillRect(60, 279, 360, 28, BG);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(failed ? DANGER : SUCCESS, BG);
      tft.drawString(message, 240, 293, 1);
    }
    return;
  }

  tft.fillRect(8, 49, 464, 263, BG);
  const uint8_t maximumOffset = count > 3 ? count - 3 : 0;
  scrollOffset = min<uint8_t>(scrollOffset, maximumOffset);
  const uint8_t visible = min<uint8_t>(count - scrollOffset, 3);
  float totalDistanceM = 0.0f;
  for (uint8_t i = 0; i < count; ++i) {
    totalDistanceM += rides[i].distanceM;
  }
  for (uint8_t i = 0; i < visible; ++i) {
    const RideSummaryItem& ride = rides[scrollOffset + i];
    const int16_t y = 57 + i * 75;
    tft.fillRoundRect(18, y, 444, 66, 12, SURFACE);
    tft.drawRoundRect(18, y, 444, 66, 12, BORDER);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(ride.complete ? TEXT : WARNING, SURFACE);
    tft.drawString("Ride " + String(ride.id), 32, y + 12, 2);
    tft.setTextColor(ACCENT, SURFACE);
    tft.drawString(String(ride.distanceM / 1000.0f, 2) + " km", 32,
                   y + 37, 4);
    tft.setTextColor(TEXT_MUTED, SURFACE);
    tft.drawString(duration(ride.elapsedMs), 181, y + 42, 1);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(TEXT, SURFACE);
    tft.drawString(String(ride.avgKmh, 1) + " km/h", 450, y + 43, 2);
    tft.setTextColor(TEXT_MUTED, SURFACE);
    tft.drawString(">", 450, y + 17, 2);
  }
  if (count > 3) {
    constexpr int16_t trackY = 57;
    constexpr int16_t trackH = 216;
    constexpr int16_t trackX = 469;
    const int16_t thumbH = max<int16_t>(28, (trackH * 3) / count);
    const int16_t thumbTravel = trackH - thumbH;
    const int16_t thumbY =
        trackY + (thumbTravel * scrollOffset) / maximumOffset;
    tft.fillRoundRect(trackX, trackY, 3, trackH, 2, SURFACE_2);
    tft.fillRoundRect(trackX, thumbY, 3, thumbH, 2, ACCENT);
  }
  tft.setTextDatum(MC_DATUM);
  if (message.length()) {
    const bool failed = message.indexOf("failed") >= 0 ||
                        message.indexOf("Cannot") >= 0;
    tft.setTextColor(failed ? DANGER : SUCCESS, BG);
    tft.drawString(message, 240, 297, 1);
  } else {
    tft.setTextColor(TEXT_MUTED, BG);
    tft.drawString(String(scrollOffset + 1) + "-" +
                       String(scrollOffset + visible) + " of " +
                       String(count) + " - " +
                       String(totalDistanceM / 1000.0f, 2) + " km",
                   240, 297, 1);
  }
}

void SecondaryScreens::historyDetail(TFT_eSPI& tft, const HeaderStatus& header,
                                     const RideSummaryItem* ride,
                                     bool storageAvailable) {
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_42_HISTORY_DETAIL);
  Components::header(tft, "Ride details", header);
  if (!ride) return;
  Components::card(tft, 18, 57, 128, 67, "DISTANCE",
                   String(ride->distanceM / 1000.0f, 2), "km", true);
  Components::card(tft, 156, 57, 128, 67, "AVG", String(ride->avgKmh, 1),
                   "km/h");
  Components::card(tft, 294, 57, 168, 67, "MOVING",
                   duration(ride->movingMs));
  tft.fillRoundRect(18, 136, 444, 101, 12, SURFACE);
  tft.drawRoundRect(18, 136, 444, 101, 12, BORDER);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TEXT_MUTED, BG);
  tft.setTextColor(TEXT_MUTED, SURFACE);
  tft.drawString("Detailed samples remain in samples.csv", 240, 178, 2);
  tft.drawString("Max " + String(ride->maxKmh, 1) + " km/h - elapsed " +
                     duration(ride->elapsedMs),
                 240, 207, 1);
  tft.fillRect(0, 246, SCREEN_W, 74, BG);
  Components::button(tft, 18, 252, 130, 50, "Delete", false, true);
  Components::button(tft, 158, 252, 146, 50, "USB Storage", false, false,
                     storageAvailable);
  Components::button(tft, 314, 252, 148, 50, "Back", true);
}

void SecondaryScreens::deleteRideConfirm(TFT_eSPI& tft) {
  for (int16_t y = 0; y < SCREEN_H; y += 3) {
    tft.drawFastHLine(0, y, SCREEN_W, TFT_BLACK);
    if (y + 1 < SCREEN_H) {
      tft.drawFastHLine(0, y + 1, SCREEN_W, TFT_BLACK);
    }
  }
  tft.fillRoundRect(54, 72, 372, 176, 16, SURFACE_2);
  tft.drawRoundRect(54, 72, 372, 176, 16, BORDER);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TEXT, SURFACE_2);
  tft.drawString("Delete ride?", 78, 90, 4);
  tft.setTextColor(TEXT_MUTED, SURFACE_2);
  tft.drawString("Ride files will be removed from SD.", 78, 126, 2);
  tft.drawString("This action cannot be undone.", 78, 149, 2);
  Components::button(tft, 78, 198, 157, 34, "Cancel");
  Components::button(tft, 245, 198, 157, 34, "Delete", false, true);
}

void SecondaryScreens::diagnostics(TFT_eSPI& tft, const HeaderStatus& header,
                                   bool sdAvailable) {
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_80_DIAGNOSTICS);
  Components::header(tft, "Diagnostics", header);
  // The source asset contains the old full-width USB control. Clear its whole
  // footer before drawing the two replacement buttons so no border or label
  // can remain visible in the gap between them.
  tft.fillRect(0, 258, 480, 62, BG);
  Components::button(tft, 18, 266, 214, 41, "Dev Mode", true, false, true);
  Components::button(tft, 248, 266, 214, 41, "USB Storage", false, false,
                     sdAvailable);
  if (!sdAvailable) {
    Components::menuTile(tft, 18, 125, 214, 56, Icon::Storage, "SD test",
                         String(), false, false);
  }
}

void SecondaryScreens::finishConfirm(TFT_eSPI& tft) {
  // Preserve the live ride values behind the modal. Two dark scanlines out of
  // every three approximate the 155/255 black overlay from the exact layout
  // without reading the TFT or allocating a second framebuffer.
  for (int16_t y = 0; y < SCREEN_H; y += 3) {
    tft.drawFastHLine(0, y, SCREEN_W, TFT_BLACK);
    if (y + 1 < SCREEN_H) {
      tft.drawFastHLine(0, y + 1, SCREEN_W, TFT_BLACK);
    }
  }
  tft.fillRoundRect(54, 72, 372, 176, 16, SURFACE_2);
  tft.drawRoundRect(54, 72, 372, 176, 16, BORDER);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TEXT, SURFACE_2);
  tft.drawString("Finish ride?", 78, 90, 4);
  tft.setTextColor(TEXT_MUTED, SURFACE_2);
  tft.drawString("The current ride will be saved to the SD", 78, 126, 2);
  tft.drawString("card and synced to the phone.", 78, 145, 2);
  Components::button(tft, 78, 198, 157, 34, "Cancel");
  Components::button(tft, 245, 198, 157, 34, "Finish", false, true);
}

void SecondaryScreens::rideSummary(TFT_eSPI& tft, const HeaderStatus& header,
                                   const RideStats& stats) {
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_18_RIDE_SUMMARY);
  Components::header(tft, "Ride complete", header);
  Components::card(tft, 24, 96, 144, 70, "DISTANCE",
                   String(stats.distanceM / 1000.0f, 2), "km", true);
  Components::card(tft, 180, 96, 144, 70, "AVG",
                   String(stats.averageMovingSpeedKmh, 1), "km/h");
  Components::card(tft, 336, 96, 120, 70, "MAX", String(stats.maxSpeedKmh, 1), "km/h");
  Components::card(tft, 24, 180, 144, 66, "MOVING", duration(stats.movingMs));
  Components::card(tft, 180, 180, 144, 66, "ELAPSED", duration(stats.elapsedMs));
  Components::card(tft, 336, 180, 120, 66, "PHONE",
                   header.phoneConnected ? "Ready" : "Offline");
}

}  // namespace ui
