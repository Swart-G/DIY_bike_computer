#include "ui/screens/HomeScreen.h"

#include "ui/UiTheme.h"
#include "ui_exact/exact_screen_renderer.h"

namespace ui {

void HomeScreen::draw(TFT_eSPI& tft, const HomeViewModel& model) {
  ui_exact::ExactScreenRenderer exact(tft);
  exact.draw(model.phoneConnected
                 ? ui_exact::ScreenId::SCREEN_04_HOME_CONNECTED
                 : ui_exact::ScreenId::SCREEN_03_HOME_DISCONNECTED);
  Components::header(tft, model.timeText, model.header);

  // Runtime state replaces only the declared dynamic regions. The sourcepack
  // remains the exact baseline for typography, icons and geometry.
  // Keep this patch below the descender of the sourcepack "Ready to ride"
  // heading. The previous y=85 rectangle clipped the tail of its final 'y'.
  tft.fillRect(22, 89, 220, 19, BG);
  tft.fillCircle(28, 94, 3, model.phoneConnected ? SUCCESS : TEXT_MUTED);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TEXT_MUTED, BG);
  tft.drawString(model.phoneConnected ? "Phone connected" : "Phone not connected", 36, 94, 1);

  String historySubtitle;
  if (!model.historyAvailable) {
    historySubtitle = "SD unavailable";
  } else if (model.rideCount == 0) {
    historySubtitle = "No rides";
  } else {
    historySubtitle = String(model.rideCount) + " rides";
  }
  if (!model.historyAvailable) {
    Components::menuTile(tft, 24, 202, 206, 66, Icon::History, "History",
                         historySubtitle, false, false);
  } else {
    tft.fillRect(82, 235, 138, 25, SURFACE);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TEXT_MUTED, SURFACE);
    tft.drawString(historySubtitle, 86, 238, 1);
  }
}

}  // namespace ui
