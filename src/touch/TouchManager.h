#pragma once

#include <Arduino.h>
#include <Wire.h>

struct TouchContact {
  bool valid = false;
  uint8_t id = 0xFF;
  uint8_t event = 0xFF;
  int16_t rawX = 0;
  int16_t rawY = 0;
  int16_t x = 0;
  int16_t y = 0;
};

struct TouchPoint {
  bool touched = false;
  uint8_t points = 0;
  int16_t rawX = 0;
  int16_t rawY = 0;
  int16_t x = 0;
  int16_t y = 0;
  bool intLevel = true;
  uint32_t lastTouchMs = 0;
  TouchContact contacts[2];
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
  const TouchContact& contact(uint8_t index) const {
    return point_.contacts[index < 2 ? index : 0];
  }

 private:
  bool readBytes(uint8_t reg, uint8_t* data, size_t len);
  bool writeByte(uint8_t reg, uint8_t value);
  void transformRaw(TouchContact& contact);
  void clearContacts();

  bool ready_ = false;
  TouchPoint point_;
  uint8_t consecutiveReadFailures_ = 0;
  uint32_t lastReadAttemptMs_ = 0;
};
