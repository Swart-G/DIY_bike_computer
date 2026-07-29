#include "display/DisplayManager.h"

#include <cstring>
#include <esp_heap_caps.h>

#include "bus/SharedSpiBus.h"
#include "config/hardware_config.h"
#include "ui/UiTheme.h"
#include "ui_exact/exact_screen_renderer.h"

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

namespace {

constexpr uint16_t kFrameTransferRows = 4;

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

void DisplayManager::showBoot(const String& line1, const String& line2) {
  hw::SharedSpiBusGuard bus;
  TFT_eSPI& gfx = tft();
  ui_exact::ExactScreenRenderer exact(gfx);
  exact.draw(ui_exact::ScreenId::SCREEN_00_BOOT);
  gfx.fillRect(90, 249, 300, 43, ui::BG);
  gfx.setTextDatum(MC_DATUM);
  gfx.setTextColor(ui::TEXT_MUTED, ui::BG);
  gfx.drawString(line1.length() ? line1 : "Starting services...",
                 width() / 2, 260, 1);
  if (line2.length() > 0) {
    gfx.drawString(line2, width() / 2, 280, 1);
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
