#include "display/DisplayManager.h"

#include <cstring>
#include <esp_heap_caps.h>

#include "config/hardware_config.h"

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

namespace {

constexpr uint16_t kFrameTransferRows = 4;

void releaseSdForDisplay() {
  digitalWrite(hw::PIN_SD_CS, HIGH);
  digitalWrite(hw::PIN_LCD_CS, HIGH);
  delayMicroseconds(2);
}

}

bool DisplayManager::begin(uint8_t brightnessPercent) {
  pinMode(hw::PIN_LCD_CS, OUTPUT);
  pinMode(hw::PIN_SD_CS, OUTPUT);
  releaseSdForDisplay();

  pinMode(hw::PIN_LCD_BACKLIGHT, OUTPUT);
  setBrightness(0);

  tft_.init();
  tft_.setRotation(hw::DISPLAY_ROTATION);
  tft_.setTextFont(2);
  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  releaseSdForDisplay();
  tft_.fillScreen(TFT_BLACK);

  ready_ = true;
  frame_.setColorDepth(16);
  frameBufferReady_ = frame_.createSprite(tft_.width(), tft_.height()) != nullptr;
  if (frameBufferReady_) {
    frame_.setTextFont(2);
    frame_.setTextColor(TFT_WHITE, TFT_BLACK);
    frame_.fillSprite(TFT_BLACK);
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
  const uint8_t duty = static_cast<uint8_t>((static_cast<uint16_t>(percent) * 255U) / 100U);

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  static bool attached = false;
  if (!attached) {
    ledcAttach(hw::PIN_LCD_BACKLIGHT, 5000, 8);
    attached = true;
  }
  ledcWrite(hw::PIN_LCD_BACKLIGHT, duty);
#else
  static bool attached = false;
  if (!attached) {
    ledcSetup(0, 5000, 8);
    ledcAttachPin(hw::PIN_LCD_BACKLIGHT, 0);
    attached = true;
  }
  ledcWrite(0, duty);
#endif
}

int16_t DisplayManager::width() {
  return ready_ ? tft_.width() : hw::DISPLAY_WIDTH;
}

int16_t DisplayManager::height() {
  return ready_ ? tft_.height() : hw::DISPLAY_HEIGHT;
}

void DisplayManager::clear(uint16_t color) {
  tft().fillScreen(color);
}

void DisplayManager::beginFrame(uint16_t color) {
  if (!frameBufferReady_) {
    frameActive_ = false;
    releaseSdForDisplay();
    tft_.startWrite();
    tft_.fillScreen(color);
    return;
  }
  frameActive_ = true;
  frame_.fillSprite(color);
}

void DisplayManager::commitFrame() {
  if (frameBufferReady_ && frameActive_) {
    uint16_t* framePixels = static_cast<uint16_t*>(frame_.getPointer());
    if (framePixels != nullptr && frameTransferBuffer_ != nullptr && frameTransferRows_ > 0) {
      const int16_t w = tft_.width();
      const int16_t h = tft_.height();
      const bool oldSwapBytes = tft_.getSwapBytes();
      releaseSdForDisplay();
      tft_.setSwapBytes(false);
      tft_.startWrite();
      tft_.setAddrWindow(0, 0, w, h);
      for (int16_t y = 0; y < h; y += frameTransferRows_) {
        const int16_t rows = min<int16_t>(frameTransferRows_, h - y);
        memcpy(frameTransferBuffer_, framePixels + static_cast<size_t>(y) * w,
               static_cast<size_t>(w) * rows * sizeof(uint16_t));
        tft_.pushPixels(frameTransferBuffer_, static_cast<uint32_t>(w) * rows);
      }
      tft_.endWrite();
      tft_.setSwapBytes(oldSwapBytes);
    } else {
      releaseSdForDisplay();
      frame_.pushSprite(0, 0);
    }
    frameActive_ = false;
    return;
  }
  if (!frameBufferReady_) {
    tft_.endWrite();
  }
}

void DisplayManager::showBoot(const String& line1, const String& line2) {
  TFT_eSPI& gfx = tft();
  gfx.fillScreen(TFT_BLACK);
  gfx.setTextDatum(MC_DATUM);
  gfx.setTextColor(TFT_CYAN, TFT_BLACK);
  gfx.drawString("Bike Speedometer", width() / 2, 82, 4);
  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.drawString(line1, width() / 2, 148, 2);
  if (line2.length() > 0) {
    gfx.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    gfx.drawString(line2, width() / 2, 178, 2);
  }
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
