#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "media/MediaState.h"
#include "navigation/NavigationState.h"
#include "speed/RideStateMachine.h"
#include "ui/components/UiComponents.h"

namespace ui {

enum class RideRenderMode : uint8_t {
  Full,
  ContentRegion,
  RainRegion,
};

struct RideViewModel {
  HeaderStatus header;
  RideState state = RideState::IDLE;
  const RideStats* stats = nullptr;
  float speedKmh = 0;
  const float* graphSamples = nullptr;
  uint8_t graphCount = 0;
  uint8_t graphStart = 0;
  uint16_t graphWindowSeconds = 60;
  uint8_t page = 0;
  uint8_t pageCount = 3;
  const media::MediaState* media = nullptr;
  const navigation::NavigationState* navigation = nullptr;
  int64_t epochNowMs = 0;
  int32_t utcOffsetSeconds = 0;
  String timeText = "--:--";
  RideRenderMode renderMode = RideRenderMode::Full;
};

class RideScreen {
 public:
  static void draw(TFT_eSPI& tft, const RideViewModel& model);

 private:
  static String duration(uint64_t ms);
  static const char* stateLabel(RideState state);
  static uint16_t stateColor(RideState state);
  static void drawSpeed(TFT_eSPI& tft, const RideViewModel& model);
  static void drawStats(TFT_eSPI& tft, const RideViewModel& model);
  static void drawGraph(TFT_eSPI& tft, const RideViewModel& model);
  static void drawNavigation(TFT_eSPI& tft, const RideViewModel& model);
  static void drawMedia(TFT_eSPI& tft, const RideViewModel& model);
  static void drawControls(TFT_eSPI& tft, RideState state);
};

}  // namespace ui
