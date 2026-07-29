#include "time/ClockManager.h"

#include <esp_timer.h>

namespace {
constexpr int64_t kMinimumEpochMs = 1577836800000LL;  // 2020-01-01 UTC
constexpr int64_t kMaximumEpochMs = 4133980800000LL;  // 2101-01-01 UTC
}

bool ClockManager::sync(int64_t unixTimeMs, int32_t utcOffsetSeconds,
                        const char* timezoneId, uint64_t monotonicNowMs) {
  if (unixTimeMs < kMinimumEpochMs || unixTimeMs >= kMaximumEpochMs ||
      utcOffsetSeconds < -86400 || utcOffsetSeconds > 86400) {
    return false;
  }
  syncEpochMs_ = unixTimeMs;
  syncMonotonicMs_ = monotonicNowMs;
  utcOffsetSeconds_ = utcOffsetSeconds;
  strlcpy(timezoneId_, timezoneId ? timezoneId : "", sizeof(timezoneId_));
  synced_ = true;
  ++generation_;
  return true;
}

int64_t ClockManager::epochNowMs(uint64_t monotonicNowMs) const {
  if (!synced_) return 0;
  return syncEpochMs_ +
         static_cast<int64_t>(monotonicNowMs - syncMonotonicMs_);
}

uint64_t ClockManager::monotonicMs() {
  return static_cast<uint64_t>(esp_timer_get_time()) / 1000ULL;
}
