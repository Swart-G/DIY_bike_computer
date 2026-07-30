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
  TFT_eSPI& tft() {
    if (frameActive_ && frameBufferReady_) {
      return static_cast<TFT_eSPI&>(frame_);
    }
    return tft_;
  }

  void clear(uint16_t color = 0x0861);
  void beginFrame(uint16_t color = 0x0861);
  void beginPartialFrame();
  void commitFrame();
  void commitFrameArea(int16_t x, int16_t y, int16_t w, int16_t h);
  void resetBoot(const String& version);
  void addBootLog(const String& label, bool ok, const String& detail = String());
  void animateBoot(uint32_t durationMs);
  void showBoot(const String& line1, const String& line2 = String());
  void drawHeader(const String& title, const String& status = String());
  void drawFooter(const String& status = String());
  void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const String& label,
                  uint16_t fillColor, uint16_t textColor = TFT_WHITE, bool enabled = true);

 private:
  static constexpr uint8_t kBootLogCapacity = 5;
  static constexpr uint8_t kBootLogLength = 34;

  void renderBootFrame(const String& line1, const String& line2);
  void renderBootAnimationFrame();

  TFT_eSPI tft_;
  TFT_eSprite frame_ = TFT_eSprite(&tft_);
  uint16_t* frameTransferBuffer_ = nullptr;
  uint16_t frameTransferRows_ = 0;
  bool ready_ = false;
  bool frameBufferReady_ = false;
  bool frameActive_ = false;
  bool directFrameBusLocked_ = false;
  uint8_t brightnessPercent_ = 0;
  uint8_t bootLogCount_ = 0;
  uint8_t bootAnimationPhase_ = 0;
  char bootVersion_[20] = {};
  char bootLogs_[kBootLogCapacity][kBootLogLength] = {};
};
