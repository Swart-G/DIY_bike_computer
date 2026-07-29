#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "ui/components/UiComponents.h"

namespace ui {

struct HomeViewModel {
  HeaderStatus header;
  bool phoneConnected = false;
  bool historyAvailable = false;
  uint8_t rideCount = 0;
  String timeText = "--:--";
};

class HomeScreen {
 public:
  static void draw(TFT_eSPI& tft, const HomeViewModel& model);
};

}  // namespace ui
