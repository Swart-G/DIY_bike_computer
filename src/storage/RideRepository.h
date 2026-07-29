#pragma once
#include <Arduino.h>
#include "speed/RideStateMachine.h"
class StorageManager;
struct RideSummaryItem {
  uint32_t id = 0;
  uint8_t formatVersion = 1;
  bool complete = false;
  float distanceM = 0, avgKmh = 0, maxKmh = 0, batteryStart = 0,
        batteryEnd = 0;
  uint64_t elapsedMs = 0, movingMs = 0;
  int64_t startedAtUtcMs = -1, finishedAtUtcMs = -1;
  char folder[32] = {0};
};
class RideRepository {
 public:
  uint8_t list(StorageManager& storage, RideSummaryItem* items, uint8_t capacity, String& error);
  bool remove(StorageManager& storage, const RideSummaryItem& item, const RideRecoveryData& active, String& error);
};
