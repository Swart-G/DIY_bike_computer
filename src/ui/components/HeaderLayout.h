#pragma once

#include <stdint.h>

namespace ui {

static constexpr int16_t kHeaderRainIconCenterX = 132;
static constexpr int16_t kHeaderRainTouchLeft = 105;
static constexpr int16_t kHeaderRainTouchWidth = 55;
static constexpr int16_t kHeaderTouchHeight = 58;

constexpr int16_t headerRainIconCenterX() {
  return kHeaderRainIconCenterX;
}

constexpr bool headerRainHit(int16_t x, int16_t y) {
  return x >= kHeaderRainTouchLeft &&
         x < kHeaderRainTouchLeft + kHeaderRainTouchWidth &&
         y >= 0 && y < kHeaderTouchHeight;
}

}  // namespace ui
