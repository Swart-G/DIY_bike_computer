#include "display/DisplayManager.h"

#include <cstring>
#include <esp_heap_caps.h>

#include "bus/SharedSpiBus.h"
#include "config/hardware_config.h"
#include "ui/UiTheme.h"
#include "ui/components/IconRenderer.h"

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

namespace {

constexpr uint16_t kFrameTransferRows = 4;
constexpr int16_t kBootAnimationX = 166;
constexpr int16_t kBootAnimationY = 43;
constexpr int16_t kBootAnimationWidth = 148;
constexpr int16_t kBootAnimationHeight = 82;

constexpr int8_t kSpokeX[16] = {
    0, 5, 8, 11, 12, 11, 8, 5, 0, -5, -8, -11, -12, -11, -8, -5,
};
constexpr int8_t kSpokeY[16] = {
    -12, -11, -8, -5, 0, 5, 8, 11, 12, 11, 8, 5, 0, -5, -8, -11,
};

void drawBootBike(TFT_eSPI& gfx, uint8_t phase) {
  constexpr int16_t cx = hw::DISPLAY_WIDTH / 2;
  constexpr int16_t cy = 78;
  constexpr int16_t leftWheelX = cx - 40;
  constexpr int16_t rightWheelX = cx + 40;
  constexpr int16_t wheelY = cy + 14;

  ui::IconRenderer::draw(gfx, ui::Icon::Bike, cx, cy, ui::ACCENT, 2);

  // Three rotating diameters produce six proper spokes per wheel. The
  // endpoints stay inside the accent wheel rim instead of covering it.
  constexpr uint8_t kSpokeOffsets[3] = {0, 3, 6};
  for (uint8_t offset : kSpokeOffsets) {
    const uint8_t first = (phase + offset) & 0x0F;
    const uint8_t opposite = (first + 8) & 0x0F;
    gfx.drawLine(leftWheelX + kSpokeX[first],
                 wheelY + kSpokeY[first],
                 leftWheelX + kSpokeX[opposite],
                 wheelY + kSpokeY[opposite], ui::TEXT_MUTED);
    gfx.drawLine(rightWheelX + kSpokeX[first],
                 wheelY + kSpokeY[first],
                 rightWheelX + kSpokeX[opposite],
                 wheelY + kSpokeY[opposite], ui::TEXT_MUTED);
  }
  gfx.fillCircle(leftWheelX, wheelY, 2, ui::TEXT);
  gfx.fillCircle(rightWheelX, wheelY, 2, ui::TEXT);

  // Moving road dashes make progress visible while the bicycle itself remains
  // stable and recognisable.
  const int16_t offset = static_cast<int16_t>((phase % 6) * 8);
  for (int16_t x = 170 - offset; x < 318; x += 48) {
    const int16_t clippedX = max<int16_t>(170, x);
    const int16_t clippedRight = min<int16_t>(310, x + 24);
    if (clippedRight > clippedX) {
      gfx.drawFastHLine(clippedX, 116, clippedRight - clippedX,
                        ui::TEXT_MUTED);
    }
  }
}

}

bool DisplayManager::begin(uint8_t brightnessPercent) {
  pinMode(hw::PIN_LCD_CS, OUTPUT);
  pinMode(hw::PIN_SD_CS, OUTPUT);
  hw::releaseSharedSpiDevices();

  pinMode(hw::PIN_LCD_BACKLIGHT, OUTPUT);
  setBrightness(0);

  {
    hw::SharedSpiBusGuard bus;
    tft_.init();
    tft_.invertDisplay(hw::DISPLAY_INVERT_COLORS);
    tft_.setRotation(hw::DISPLAY_ROTATION);
    tft_.setTextFont(2);
    tft_.setTextColor(TFT_WHITE, ui::BG);
    tft_.fillScreen(ui::BG);
  }

  ready_ = true;
  frame_.setColorDepth(16);
  frameBufferReady_ = frame_.createSprite(tft_.width(), tft_.height()) != nullptr;
  if (frameBufferReady_) {
    frame_.setTextFont(2);
    frame_.setTextColor(TFT_WHITE, ui::BG);
    frame_.fillSprite(ui::BG);
    frameTransferRows_ = kFrameTransferRows;
    frameTransferBuffer_ = static_cast<uint16_t*>(
        heap_caps_malloc(tft_.width() * frameTransferRows_ * sizeof(uint16_t),
                         MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (frameTransferBuffer_ == nullptr) {
      frameTransferRows_ = 0;
    }
  }
  setBrightness(brightnessPercent);
  return ready_;
}

void DisplayManager::setBrightness(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  brightnessPercent_ = percent;
  uint8_t duty = static_cast<uint8_t>((static_cast<uint16_t>(percent) * 255U) / 100U);
  if (!hw::LCD_BACKLIGHT_ACTIVE_HIGH) {
    duty = 255U - duty;
  }

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  static bool attached = false;
  if (!attached) {
    ledcAttach(hw::PIN_LCD_BACKLIGHT, hw::LCD_BACKLIGHT_PWM_FREQUENCY_HZ,
               hw::LCD_BACKLIGHT_PWM_RESOLUTION_BITS);
    attached = true;
  }
  ledcWrite(hw::PIN_LCD_BACKLIGHT, duty);
#else
  static bool attached = false;
  if (!attached) {
    ledcSetup(hw::LCD_BACKLIGHT_PWM_CHANNEL, hw::LCD_BACKLIGHT_PWM_FREQUENCY_HZ,
              hw::LCD_BACKLIGHT_PWM_RESOLUTION_BITS);
    ledcAttachPin(hw::PIN_LCD_BACKLIGHT, hw::LCD_BACKLIGHT_PWM_CHANNEL);
    attached = true;
  }
  ledcWrite(hw::LCD_BACKLIGHT_PWM_CHANNEL, duty);
#endif
}

int16_t DisplayManager::width() {
  return ready_ ? tft_.width() : hw::DISPLAY_WIDTH;
}

int16_t DisplayManager::height() {
  return ready_ ? tft_.height() : hw::DISPLAY_HEIGHT;
}

void DisplayManager::clear(uint16_t color) {
  if (frameActive_ && frameBufferReady_) {
    frame_.fillSprite(color);
    return;
  }
  hw::SharedSpiBusGuard bus;
  tft_.fillScreen(color);
}

void DisplayManager::beginFrame(uint16_t color) {
  if (!frameBufferReady_) {
    frameActive_ = false;
    hw::lockSharedSpiBus();
    directFrameBusLocked_ = true;
    hw::releaseSharedSpiDevices();
    tft_.startWrite();
    tft_.fillScreen(color);
    return;
  }
  frameActive_ = true;
  frame_.fillSprite(color);
}

void DisplayManager::beginPartialFrame() {
  if (!frameBufferReady_) {
    beginFrame();
    return;
  }
  frameActive_ = true;
}

void DisplayManager::commitFrame() {
  if (frameBufferReady_ && frameActive_) {
    if (!ready_) {
      frameActive_ = false;
      return;
    }
    uint16_t* framePixels = static_cast<uint16_t*>(frame_.getPointer());
    if (framePixels != nullptr && frameTransferBuffer_ != nullptr && frameTransferRows_ > 0) {
      const int16_t w = tft_.width();
      const int16_t h = tft_.height();
      const bool oldSwapBytes = tft_.getSwapBytes();
      hw::SharedSpiBusGuard bus;
      tft_.setSwapBytes(false);
      tft_.startWrite();
      tft_.setAddrWindow(0, 0, w, h);
      for (int16_t y = 0; y < h; y += frameTransferRows_) {
        const int16_t rows = min<int16_t>(frameTransferRows_, h - y);
        const size_t pixelCount = static_cast<size_t>(w) * rows;
        memcpy(frameTransferBuffer_, framePixels + static_cast<size_t>(y) * w,
               pixelCount * sizeof(uint16_t));
        tft_.pushPixels(frameTransferBuffer_, static_cast<uint32_t>(pixelCount));
      }
      tft_.endWrite();
      tft_.setSwapBytes(oldSwapBytes);
    } else {
      hw::SharedSpiBusGuard bus;
      frame_.pushSprite(0, 0);
    }
    frameActive_ = false;
    return;
  }
  if (!frameBufferReady_) {
    tft_.endWrite();
    if (directFrameBusLocked_) {
      hw::releaseSharedSpiDevices();
      hw::unlockSharedSpiBus();
      directFrameBusLocked_ = false;
    }
  }
}

void DisplayManager::commitFrameArea(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (!frameBufferReady_ || !frameActive_ || frameTransferBuffer_ == nullptr ||
      frameTransferRows_ == 0) {
    commitFrame();
    return;
  }
  if (!ready_) {
    frameActive_ = false;
    return;
  }
  x = constrain(x, 0, tft_.width() - 1);
  y = constrain(y, 0, tft_.height() - 1);
  w = constrain(w, 1, tft_.width() - x);
  h = constrain(h, 1, tft_.height() - y);
  uint16_t* framePixels = static_cast<uint16_t*>(frame_.getPointer());
  if (framePixels == nullptr) {
    commitFrame();
    return;
  }

  const int16_t frameWidth = tft_.width();
  const bool oldSwapBytes = tft_.getSwapBytes();
  hw::SharedSpiBusGuard bus;
  tft_.setSwapBytes(false);
  tft_.startWrite();
  tft_.setAddrWindow(x, y, w, h);
  for (int16_t row = 0; row < h; row += frameTransferRows_) {
    const int16_t rows = min<int16_t>(frameTransferRows_, h - row);
    for (int16_t localRow = 0; localRow < rows; ++localRow) {
      memcpy(frameTransferBuffer_ + static_cast<size_t>(localRow) * w,
             framePixels + static_cast<size_t>(y + row + localRow) * frameWidth + x,
             static_cast<size_t>(w) * sizeof(uint16_t));
    }
    tft_.pushPixels(frameTransferBuffer_, static_cast<uint32_t>(w) * rows);
  }
  tft_.endWrite();
  tft_.setSwapBytes(oldSwapBytes);
  frameActive_ = false;
}

void DisplayManager::resetBoot(const String& version) {
  bootLogCount_ = 0;
  bootAnimationPhase_ = 0;
  memset(bootLogs_, 0, sizeof(bootLogs_));
  version.substring(0, sizeof(bootVersion_) - 1).toCharArray(bootVersion_, sizeof(bootVersion_));
  renderBootFrame("Starting services...", String());
}

void DisplayManager::addBootLog(const String& label, bool ok, const String& detail) {
  char line[kBootLogLength] = {};
  const String message = String(ok ? "[OK] " : "[!!] ") + label +
                         (detail.length() ? String("  ") + detail : String());
  message.substring(0, sizeof(line) - 1).toCharArray(line, sizeof(line));

  if (bootLogCount_ < kBootLogCapacity) {
    strncpy(bootLogs_[bootLogCount_++], line, kBootLogLength - 1);
  } else {
    for (uint8_t i = 1; i < kBootLogCapacity; ++i) {
      memcpy(bootLogs_[i - 1], bootLogs_[i], kBootLogLength);
    }
    strncpy(bootLogs_[kBootLogCapacity - 1], line, kBootLogLength - 1);
  }
  ++bootAnimationPhase_;
  renderBootFrame(label, detail);
}

void DisplayManager::animateBoot(uint32_t durationMs) {
  const uint32_t startedMs = millis();
  do {
    ++bootAnimationPhase_;
    renderBootAnimationFrame();
    delay(45);
  } while (millis() - startedMs < durationMs);
}

void DisplayManager::renderBootFrame(const String& line1, const String& line2) {
  beginFrame(ui::BG);
  TFT_eSPI& gfx = tft();

  const int16_t cx = width() / 2;
  drawBootBike(gfx, bootAnimationPhase_);

  gfx.setTextDatum(MC_DATUM);
  gfx.setTextColor(ui::TEXT, ui::BG);
  gfx.drawString("BIKE COMPUTER", cx, 145, 4);
  gfx.setTextColor(ui::TEXT_MUTED, ui::BG);
  gfx.drawString(bootVersion_[0] ? bootVersion_ : "firmware", cx, 172, 2);

  gfx.fillRoundRect(70, 194, 340, 82, 10, ui::SURFACE);
  gfx.setTextDatum(ML_DATUM);
  for (uint8_t i = 0; i < bootLogCount_; ++i) {
    const uint16_t color = strncmp(bootLogs_[i], "[OK]", 4) == 0 ? ui::SUCCESS : ui::WARNING;
    gfx.setTextColor(color, ui::SURFACE);
    gfx.drawString(bootLogs_[i], 84, 204 + i * 14, 1);
  }

  gfx.setTextDatum(MC_DATUM);
  gfx.setTextColor(ui::TEXT_MUTED, ui::BG);
  gfx.drawString(line1.length() ? line1 : "Starting services...", cx, 294, 1);
  if (line2.length()) {
    gfx.setTextDatum(MR_DATUM);
    gfx.drawString(line2.substring(0, 24), width() - 18, 309, 1);
  }

  const int16_t progressWidth = min<int16_t>(444, 28 + bootLogCount_ * 72);
  gfx.fillRoundRect(18, 313, 444, 4, 2, ui::SURFACE);
  gfx.fillRoundRect(18, 313, progressWidth, 4, 2, ui::ACCENT);
  commitFrame();
}

void DisplayManager::renderBootAnimationFrame() {
  if (!frameBufferReady_ || !ready_) {
    // Without the PSRAM sprite a partial animation would expose every erase
    // and redraw over SPI. Keep the last complete frame instead of blinking.
    return;
  }

  beginPartialFrame();
  TFT_eSPI& gfx = tft();
  gfx.fillRect(kBootAnimationX, kBootAnimationY, kBootAnimationWidth,
               kBootAnimationHeight, ui::BG);
  drawBootBike(gfx, bootAnimationPhase_);
  commitFrameArea(kBootAnimationX, kBootAnimationY, kBootAnimationWidth,
                  kBootAnimationHeight);
}

void DisplayManager::showBoot(const String& line1, const String& line2) {
  ++bootAnimationPhase_;
  renderBootFrame(line1, line2);
}

void DisplayManager::drawHeader(const String& title, const String& status) {
  TFT_eSPI& gfx = tft();
  gfx.fillRect(0, 0, width(), 34, TFT_DARKGREY);
  gfx.setTextDatum(ML_DATUM);
  gfx.setTextColor(TFT_WHITE, TFT_DARKGREY);
  gfx.drawString(title, 10, 17, 2);
  if (status.length() > 0) {
    gfx.setTextDatum(MR_DATUM);
    gfx.setTextColor(TFT_YELLOW, TFT_DARKGREY);
    gfx.drawString(status, width() - 10, 17, 2);
  }
}

void DisplayManager::drawFooter(const String& status) {
  TFT_eSPI& gfx = tft();
  gfx.fillRect(0, height() - 24, width(), 24, TFT_DARKGREY);
  if (status.length() > 0) {
    gfx.setTextDatum(ML_DATUM);
    gfx.setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
    gfx.drawString(status, 10, height() - 12, 2);
  }
}

void DisplayManager::drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const String& label,
                                uint16_t fillColor, uint16_t textColor, bool enabled) {
  const uint16_t bg = enabled ? fillColor : TFT_DARKGREY;
  const uint16_t fg = enabled ? textColor : TFT_LIGHTGREY;
  TFT_eSPI& gfx = tft();
  gfx.fillRoundRect(x, y, w, h, 6, bg);
  gfx.drawRoundRect(x, y, w, h, 6, TFT_WHITE);
  gfx.setTextDatum(MC_DATUM);
  gfx.setTextColor(fg, bg);
  gfx.drawString(label, x + w / 2, y + h / 2, 2);
}
