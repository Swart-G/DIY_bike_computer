#include <TFT_eSPI.h>
#include "ui_exact/exact_screen_renderer.h"

TFT_eSPI tft;
ui_exact::ExactScreenRenderer exactRenderer(tft);

void setup() {
  tft.init();
  tft.setRotation(1);
  exactRenderer.draw(ui_exact::ScreenId::SCREEN_11_RIDE_SPEED_ACTIVE);
}

void loop() {}
