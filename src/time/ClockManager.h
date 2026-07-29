#pragma once

#include <Arduino.h>

class ClockManager {
 public:
  bool sync(int64_t unixTimeMs, int32_t utcOffsetSeconds,
            const char* timezoneId, uint64_t monotonicNowMs);
  bool synced() const { return synced_; }
  int64_t epochNowMs(uint64_t monotonicNowMs) const;
  int32_t utcOffsetSeconds() const { return utcOffsetSeconds_; }
  const char* timezoneId() const { return timezoneId_; }
  uint32_t generation() const { return generation_; }
  uint64_t lastSyncMonotonicMs() const { return syncMonotonicMs_; }

  static uint64_t monotonicMs();

 private:
  bool synced_ = false;
  int64_t syncEpochMs_ = 0;
  uint64_t syncMonotonicMs_ = 0;
  int32_t utcOffsetSeconds_ = 0;
  char timezoneId_[48] = {0};
  uint32_t generation_ = 0;
};
