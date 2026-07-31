#include "ui/screens/RideScreen.h"

#include <algorithm>
#include <math.h>

#include "ui/UiTheme.h"
#include "ui_exact/exact_screen_renderer.h"

namespace ui {

namespace {

String fitText(TFT_eSPI& tft, const char* raw, int16_t maxWidth,
               uint8_t font) {
  String value = raw && raw[0] ? String(raw) : String();
  if (tft.textWidth(value, font) <= maxWidth) return value;
  while (value.length() && tft.textWidth(value + "...", font) > maxWidth) {
    value.remove(value.length() - 1);
  }
  return value + "...";
}

String distanceText(uint32_t meters) {
  if (meters < 1000) return String(meters) + " m";
  return String(meters / 1000.0f, meters < 10000 ? 1 : 0) + " km";
}

const char* maneuverLabel(navigation::Maneuver maneuver) {
  switch (maneuver) {
    case navigation::Maneuver::Straight: return "Continue straight";
    case navigation::Maneuver::TurnLeft: return "Turn left";
    case navigation::Maneuver::TurnRight: return "Turn right";
    case navigation::Maneuver::SlightLeft: return "Slight left";
    case navigation::Maneuver::SlightRight: return "Slight right";
    case navigation::Maneuver::SharpLeft: return "Sharp left";
    case navigation::Maneuver::SharpRight: return "Sharp right";
    case navigation::Maneuver::Uturn: return "Make a U-turn";
    case navigation::Maneuver::Roundabout: return "Enter roundabout";
    case navigation::Maneuver::RoundaboutExit: return "Exit roundabout";
    case navigation::Maneuver::Destination: return "Destination";
    default: return "Follow navigation";
  }
}

String mediaTime(uint64_t ms) {
  const uint64_t totalSeconds = ms / 1000ULL;
  char text[16];
  snprintf(text, sizeof(text), "%llu:%02llu",
           static_cast<unsigned long long>(totalSeconds / 60ULL),
           static_cast<unsigned long long>(totalSeconds % 60ULL));
  return String(text);
}

ui_exact::ScreenId sourceScreen(const RideViewModel& model) {
  if (model.page == 0) {
    if (model.state == RideState::PAUSED) {
      return ui_exact::ScreenId::SCREEN_12_RIDE_SPEED_PAUSED;
    }
    if (model.state == RideState::IDLE ||
        model.state == RideState::FINISHED) {
      return ui_exact::ScreenId::SCREEN_10_RIDE_IDLE;
    }
    return ui_exact::ScreenId::SCREEN_11_RIDE_SPEED_ACTIVE;
  }
  if (model.page == 1) {
    return ui_exact::ScreenId::SCREEN_13_RIDE_STATS;
  }
  if (model.page == 2) {
    return ui_exact::ScreenId::SCREEN_14_RIDE_GRAPH;
  }
  const uint8_t trendPages = model.trendPageEnabled ? 1 : 0;
  if (model.trendPageEnabled && model.page == 3) {
    return ui_exact::ScreenId::SCREEN_14_RIDE_GRAPH;
  }
  if (model.navigation && model.navigation->available &&
      model.page == 3 + trendPages) {
    return ui_exact::ScreenId::SCREEN_15_RIDE_NAVIGATION;
  }
  return ui_exact::ScreenId::SCREEN_16_RIDE_MEDIA;
}

}  // namespace

String RideScreen::duration(uint64_t ms) {
  const uint64_t seconds = ms / 1000ULL;
  const uint64_t hours = seconds / 3600ULL;
  char text[16];
  if (hours) {
    snprintf(text, sizeof(text), "%02llu:%02llu:%02llu",
             static_cast<unsigned long long>(hours),
             static_cast<unsigned long long>((seconds / 60ULL) % 60ULL),
             static_cast<unsigned long long>(seconds % 60ULL));
  } else {
    snprintf(text, sizeof(text), "%02llu:%02llu",
             static_cast<unsigned long long>(seconds / 60ULL),
             static_cast<unsigned long long>(seconds % 60ULL));
  }
  return String(text);
}

const char* RideScreen::stateLabel(RideState state) {
  switch (state) {
    case RideState::RIDING: return "RIDING";
    case RideState::PAUSED: return "PAUSED";
    case RideState::FINISHED: return "FINISHED";
    default: return "READY";
  }
}

uint16_t RideScreen::stateColor(RideState state) {
  if (state == RideState::RIDING) return SUCCESS;
  if (state == RideState::PAUSED) return ORANGE;
  return TEXT_MUTED;
}

void RideScreen::draw(TFT_eSPI& tft, const RideViewModel& model) {
  ui_exact::ExactScreenRenderer exact(tft);
  const ui_exact::ScreenId source = sourceScreen(model);
  switch (model.renderMode) {
    case RideRenderMode::ContentRegion:
      exact.drawRegion(source, 18, 44, 444, 214);
      break;
    case RideRenderMode::RainRegion:
      exact.drawRegion(source, 60, 60, 360, 186);
      break;
    case RideRenderMode::Full:
    default:
      exact.draw(source);
      Components::header(tft, model.timeText, model.header);
      break;
  }
  Components::stateChip(tft, stateLabel(model.state), stateColor(model.state));
  if (model.page == 0) {
    drawSpeed(tft, model);
  } else if (model.page == 1) {
    drawStats(tft, model);
  } else if (model.page == 2) {
    drawGraph(tft, model);
  } else if (model.trendPageEnabled && model.page == 3) {
    drawSpeedTrend(tft, model);
  } else if (model.navigation && model.navigation->available &&
             model.page == 3 + (model.trendPageEnabled ? 1 : 0)) {
    drawNavigation(tft, model);
  } else {
    drawMedia(tft, model);
  }
  if (model.state == RideState::PAUSED && model.page == 0) {
    tft.fillRoundRect(183, 74, 114, 27, 13, ACCENT_DIM);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ACCENT, ACCENT_DIM);
    tft.drawString("PAUSED", 240, 87, 2);
  }
  Components::pageDots(tft, model.page, model.pageCount, 291);
  drawControls(tft, model.state);
}

void RideScreen::drawNavigation(TFT_eSPI& tft,
                                const RideViewModel& model) {
  const navigation::NavigationState& nav = *model.navigation;
  tft.fillRoundRect(24, 67, 432, 159, 16, SURFACE);
  tft.drawRoundRect(24, 67, 432, 159, 16, BORDER);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TEXT_MUTED, SURFACE);
  tft.drawString(nav.lifecycle == navigation::Lifecycle::Rerouting
                     ? "REROUTING"
                     : "NEXT TURN",
                 44, 84, 1);
  IconRenderer::drawManeuver(tft, nav.maneuver, 78, 122, ACCENT);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TEXT, SURFACE);
  tft.drawString(distanceText(nav.distanceToManeuverM), 143, 119, 6);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(maneuverLabel(nav.maneuver), 44, 151, 4);
  tft.setTextColor(TEXT_MUTED, SURFACE);
  tft.drawString(fitText(tft, nav.street, 388, 2), 44, 183, 2);

  String footer = distanceText(nav.remainingDistanceM) + " left";
  if (nav.etaUtcMs > 0) {
    int64_t localSeconds =
        nav.etaUtcMs / 1000LL + model.utcOffsetSeconds;
    localSeconds %= 86400LL;
    if (localSeconds < 0) localSeconds += 86400LL;
    char eta[12];
    snprintf(eta, sizeof(eta), "%02lld:%02lld",
             static_cast<long long>(localSeconds / 3600LL),
             static_cast<long long>((localSeconds / 60LL) % 60LL));
    footer += "   -   ETA ";
    footer += eta;
  }
  if (nav.nextDistanceM) {
    footer += "   -   then ";
    footer += maneuverLabel(nav.nextManeuver);
    footer += " in ";
    footer += distanceText(nav.nextDistanceM);
  }
  tft.drawString(fitText(tft, footer.c_str(), 388, 1), 44, 209, 1);
}

void RideScreen::drawMedia(TFT_eSPI& tft, const RideViewModel& model) {
  if (!model.media || !model.mediaPageEnabled) return;
  const media::MediaState& state = *model.media;
  tft.fillRect(18, 61, 444, 194, BG);
  if (!state.available) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TEXT, BG);
    tft.drawString("Nothing playing", 240, 116, 4);
    tft.setTextColor(TEXT_MUTED, BG);
    tft.drawString("Start playback or grant media access", 240, 150, 2);
    tft.drawString("in the Android companion.", 240, 173, 2);
    return;
  }
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TEXT_MUTED, BG);
  tft.drawString("Now playing", 24, 67, 1);
  tft.setTextColor(TEXT, BG);
  tft.drawString(fitText(tft, state.title[0] ? state.title : "Nothing playing",
                         432, 4),
                 24, 90, 4);
  String subtitle = state.artist;
  if (state.player[0]) {
    if (subtitle.length()) subtitle += " - ";
    subtitle += state.player;
  }
  tft.setTextColor(TEXT_MUTED, BG);
  tft.drawString(fitText(tft, subtitle.c_str(), 432, 2), 24, 124, 2);

  const uint64_t position = state.positionNow(millis());
  const uint32_t width =
      state.durationMs
          ? static_cast<uint32_t>(
                std::min<uint64_t>(432,
                                   (position * 432ULL) / state.durationMs))
          : 0;
  tft.fillRect(24, 164, 432, 4, BORDER);
  if (width) tft.fillRect(24, 164, width, 4, ACCENT);
  tft.fillCircle(24 + width, 166, 5, state.durationMs ? ACCENT : BORDER);
  tft.setTextColor(TEXT_MUTED, BG);
  tft.drawString(mediaTime(position), 24, 178, 1);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(mediaTime(state.durationMs), 456, 178, 1);

  const bool previous =
      (state.supportedActions & media::ActionMask::Previous) != 0;
  const bool toggle =
      (state.supportedActions &
       (media::ActionMask::Toggle | media::ActionMask::Play |
        media::ActionMask::Pause)) != 0;
  const bool next = (state.supportedActions & media::ActionMask::Next) != 0;
  Components::button(tft, 82, 201, 92, 47, "", false, false, previous);
  Components::button(tft, 190, 194, 100, 61, "", true, false, toggle);
  Components::button(tft, 306, 201, 92, 47, "", false, false, next);
  IconRenderer::draw(tft, Icon::MediaPrevious, 128, 224,
                     previous ? TEXT : TEXT_MUTED);
  IconRenderer::draw(tft,
                     state.playing ? Icon::MediaPause : Icon::MediaPlay, 240,
                     224, toggle ? BG : TEXT_MUTED);
  IconRenderer::draw(tft, Icon::MediaNext, 352, 224,
                     next ? TEXT : TEXT_MUTED);
}

void RideScreen::drawSpeed(TFT_eSPI& tft, const RideViewModel& model) {
  static const RideStats empty{};
  const RideStats& stats = model.stats ? *model.stats : empty;
  // Clear the complete sourcepack speed glyph box. The previous rectangle
  // started at y=104, leaving the top edges of a wider prior value visible
  // when the new speed dropped below 10 km/h.
  tft.fillRect(60, 82, 360, 110, BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TEXT, BG);
  tft.drawString(String(model.speedKmh, 1), 240, 125, 8);
  tft.setTextColor(TEXT_MUTED, BG);
  tft.drawString("km/h", 240, 175, 2);

  const bool idle = model.state == RideState::IDLE ||
                    model.state == RideState::FINISHED;
  const int16_t xs[] = {static_cast<int16_t>(idle ? 122 : 98), 240,
                        static_cast<int16_t>(idle ? 358 : 382)};
  const String values[] = {
      String(stats.distanceM / 1000.0f, 2),
      String(stats.averageMovingSpeedKmh, 1), duration(stats.movingMs)};
  const char* labels[] = {"km", "avg km/h", "moving"};
  for (uint8_t i = 0; i < 3; ++i) {
    if (idle && i == 1) continue;
    tft.fillRect(xs[i] - 58, 198, 116, 43, BG);
    tft.setTextColor(TEXT, BG);
    tft.drawString(values[i], xs[i], 214, 4);
    tft.setTextColor(TEXT_MUTED, BG);
    tft.drawString(labels[i], xs[i], 237, 1);
  }
}

void RideScreen::drawStats(TFT_eSPI& tft, const RideViewModel& model) {
  static const RideStats empty{};
  const RideStats& stats = model.stats ? *model.stats : empty;
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TEXT, BG);
  tft.drawString("Ride stats", 24, 67, 4);
  Components::card(tft, 24, 96, 144, 68, "AVG SPEED",
                   String(stats.averageMovingSpeedKmh, 1), "km/h", true);
  Components::card(tft, 180, 96, 144, 68, "MAX SPEED", String(stats.maxSpeedKmh, 1), "km/h");
  Components::card(tft, 336, 96, 120, 68, "DISTANCE", String(stats.distanceM / 1000.0f, 2),
                   "km");
  Components::card(tft, 24, 176, 144, 68, "MOVING", duration(stats.movingMs));
  Components::card(tft, 180, 176, 144, 68, "ELAPSED", duration(stats.elapsedMs));
  Components::card(tft, 336, 176, 120, 68, "PULSES", String(stats.pulseCount));
}

void RideScreen::drawGraph(TFT_eSPI& tft, const RideViewModel& model) {
  tft.fillRect(18, 61, 444, 190, BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TEXT, BG);
  tft.drawString("Speed - last " + String(model.graphWindowSeconds) + " s", 24, 67, 2);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(ACCENT, BG);
  tft.drawString(String(model.speedKmh, 1) + " km/h", 452, 67, 2);

  constexpr int16_t gx = 38;
  constexpr int16_t gy = 92;
  constexpr int16_t gw = 414;
  constexpr int16_t gh = 148;
  float maximum = 10.0f;
  for (uint8_t i = 0; i < model.graphCount; ++i) {
    if (model.graphSamples && model.graphSamples[i] > maximum) maximum = model.graphSamples[i];
  }
  maximum = ceilf(maximum / 10.0f) * 10.0f;
  for (uint8_t i = 0; i <= 3; ++i) {
    const int16_t y = gy + (i * gh) / 3;
    tft.drawFastHLine(gx, y, gw, BORDER);
  }
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(TEXT_MUTED, BG);
  tft.drawString(String(static_cast<int>(maximum)), 30, gy, 1);
  tft.drawString(String(static_cast<int>(maximum / 2.0f)), 30,
                 gy + gh / 2, 1);
  tft.drawString("0", 30, gy + gh, 1);
  for (uint8_t i = 0; i <= 3; ++i) {
    const int16_t x = gx + (i * gw) / 3;
    tft.drawFastVLine(x, gy, gh, SURFACE_2);
  }
  if (!model.graphSamples || model.graphCount < 2) return;
  int16_t px = gx;
  int16_t py = gy + gh;
  for (uint8_t i = 0; i < model.graphCount; ++i) {
    const uint8_t idx = (model.graphStart + i) % model.graphCount;
    const float sample = constrain(model.graphSamples[idx], 0.0f, maximum);
    const int16_t x = gx + (i * gw) / (model.graphCount - 1);
    const int16_t y = gy + gh - static_cast<int16_t>((sample / maximum) * gh);
    if (i) {
      tft.drawLine(px, py, x, y, ACCENT);
      tft.drawLine(px, py + 1, x, y + 1, ACCENT);
    }
    px = x;
    py = y;
  }
}

void RideScreen::drawSpeedTrend(TFT_eSPI& tft,
                                const RideViewModel& model) {
  tft.fillRect(18, 61, 444, 190, BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TEXT_MUTED, BG);
  tft.drawString("SPEED TREND", 24, 67, 1);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(TEXT, BG);
  tft.drawString(String(model.speedKmh, 1), 418, 62, 6);
  tft.setTextColor(TEXT_MUTED, BG);
  tft.drawString("km/h", 456, 91, 1);

  const SpeedTrendSnapshot empty;
  const SpeedTrendSnapshot& trend =
      model.speedTrend ? *model.speedTrend : empty;
  constexpr int16_t segmentX[] = {24, 174, 324};
  constexpr int16_t segmentW = 132;
  constexpr int16_t segmentY = 111;
  constexpr int16_t segmentH = 126;
  const char* labels[] = {"2 SEC", "5 SEC", "10 SEC"};

  for (uint8_t i = 0; i < 3; ++i) {
    const SpeedTrendReading& reading = trend.readings[i];
    uint16_t color = SUCCESS;
    const char* stateText = "STEADY";
    if (reading.state == SpeedTrendState::Accelerating) {
      color = PURPLE;
      stateText = "FASTER";
    } else if (reading.state == SpeedTrendState::Decelerating) {
      color = DANGER;
      stateText = "SLOWER";
    }

    tft.fillRoundRect(segmentX[i], segmentY, segmentW, segmentH, 12,
                      SURFACE);
    tft.drawRoundRect(segmentX[i], segmentY, segmentW, segmentH, 12, color);
    tft.fillRoundRect(segmentX[i] + 1, segmentY + 1, segmentW - 2, 13, 10,
                      color);
    tft.fillRect(segmentX[i] + 1, segmentY + 8, segmentW - 2, 7, color);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TEXT_MUTED, SURFACE);
    tft.drawString(labels[i], segmentX[i] + segmentW / 2, segmentY + 32, 1);
    tft.setTextColor(color, SURFACE);
    const String delta =
        reading.ready
            ? String(reading.deltaKmh >= 0.0f ? "+" : "") +
                  String(reading.deltaKmh, 1)
            : String("--");
    tft.drawString(delta, segmentX[i] + segmentW / 2, segmentY + 66, 4);
    tft.setTextColor(TEXT_MUTED, SURFACE);
    tft.drawString(reading.ready ? "km/h" : "collecting",
                   segmentX[i] + segmentW / 2, segmentY + 89, 1);
    tft.setTextColor(color, SURFACE);
    tft.drawString(reading.ready ? stateText : "WAIT",
                   segmentX[i] + segmentW / 2, segmentY + 109, 2);
  }
}

void RideScreen::drawControls(TFT_eSPI& tft, RideState state) {
  if (state == RideState::IDLE || state == RideState::FINISHED) {
    Components::button(tft, 24, 264, 432, 43,
                       state == RideState::FINISHED ? "NEW RIDE" : "START RIDE", true);
    return;
  }
  Components::button(tft, 24, 266, 291, 41,
                     state == RideState::PAUSED ? "RESUME" : "PAUSE", true);
  Components::button(tft, 327, 266, 129, 41, "Finish", false, true);
}

}  // namespace ui
