#pragma once

#include <Arduino.h>

namespace hw {

void lockSharedSpiBus();
void unlockSharedSpiBus();
void releaseSharedSpiDevices();

class SharedSpiBusGuard {
 public:
  explicit SharedSpiBusGuard(bool parkDisplayForSd = false);
  ~SharedSpiBusGuard();

  SharedSpiBusGuard(const SharedSpiBusGuard&) = delete;
  SharedSpiBusGuard& operator=(const SharedSpiBusGuard&) = delete;

 private:
  bool locked_ = false;
};

}  // namespace hw
