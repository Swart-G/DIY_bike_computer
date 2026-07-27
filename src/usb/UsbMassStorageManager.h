#pragma once

#include <Arduino.h>

#include "storage/StorageManager.h"

class UsbMassStorageManager {
 public:
  bool begin(StorageManager& storage);
  void end(StorageManager& storage);
  bool active() const { return active_; }
  const String& status() const { return status_; }

 private:
  bool active_ = false;
  String status_ = "USB Mass Storage not started";
};
