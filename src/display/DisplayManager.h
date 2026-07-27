#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

class DisplayManager {
 public:
  bool begin(uint8_t brightnessPercent);
  void setBrightness(uint8_t percent);
  uint8_t brightness() const { return brightnessPercent_; }
  bool isReady() const { return ready_; }
  bool frameBufferReady() const { return frameBufferReady_; }
  bool frameTransferBufferReady() const { return frameTransferBuffer_ != nullptr; }

  int16_t width();
  int16_t height();
  TFT_eSPI& tft() { return frameActive_ && frameBufferReady_ ? frame_ : tft_; }

  void clear(uint16_t color = TFT_BLACK);
  void beginFrame(uint16_t color = TFT_BLACK);
  void commitFrame();
  void showBoot(const String& line1, const String& line2 = String());
  void drawHeader(const String& title, const String& status = String());
  void drawFooter(const String& status = String());
  void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const String& label,
                  uint16_t fillColor, uint16_t textColor = TFT_WHITE, bool enabled = true);

 private:
  TFT_eSPI tft_;
  TFT_eSprite frame_ = TFT_eSprite(&tft_);
  uint16_t* frameTransferBuffer_ = nullptr;
  uint16_t frameTransferRows_ = 0;
  bool ready_ = false;
  bool frameBufferReady_ = false;
  bool frameActive_ = false;
  bool directFrameBusLocked_ = false;
  uint8_t brightnessPercent_ = 0;
};
