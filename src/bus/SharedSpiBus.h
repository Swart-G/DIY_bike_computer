#pragma once

#include <Arduino.h>

namespace hw {

void lockSharedSpiBus();
void unlockSharedSpiBus();
void configureSharedSpiChipSelects();
void releaseSharedSpiDevices();
void clockSdCardIdle(uint16_t byteCount = 2);
bool abortSdTransfer();

class SharedSpiBusGuard {
 public:
  explicit SharedSpiBusGuard(bool parkDisplayForSd = false);
  ~SharedSpiBusGuard();

  SharedSpiBusGuard(const SharedSpiBusGuard&) = delete;
  SharedSpiBusGuard& operator=(const SharedSpiBusGuard&) = delete;

 private:
  bool locked_ = false;
  bool sdTraffic_ = false;
};

}  // namespace hw
