#pragma once

#include <Arduino.h>
#include <Wire.h>

struct TouchPoint {
  bool touched = false;
  uint8_t points = 0;
  int16_t rawX = 0;
  int16_t rawY = 0;
  int16_t x = 0;
  int16_t y = 0;
  bool intLevel = true;
  uint32_t lastTouchMs = 0;
};

class TouchManager {
 public:
  bool begin();
  bool update();
  bool isReady() const { return ready_; }
  const TouchPoint& point() const { return point_; }
  bool touched() const { return point_.touched; }
  int16_t x() const { return point_.x; }
  int16_t y() const { return point_.y; }

 private:
  bool readBytes(uint8_t reg, uint8_t* data, size_t len);
  bool writeByte(uint8_t reg, uint8_t value);
  void transformRaw();

  bool ready_ = false;
  TouchPoint point_;
  uint8_t consecutiveReadFailures_ = 0;
  uint32_t lastReadAttemptMs_ = 0;
};
