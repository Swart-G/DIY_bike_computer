#include "storage/RideRepository.h"

#include <ArduinoJson.h>
#include <SD.h>

#include "bus/SharedSpiBus.h"
#include "storage/StorageManager.h"

namespace {

class Guard {
 public:
  Guard() : guard_(true) {}

 private:
  hw::SharedSpiBusGuard guard_;
};

bool isEnumeratedRideFolder(const char* path) {
  constexpr char kPrefix[] = "/rides/";
  if (!path || strncmp(path, kPrefix, sizeof(kPrefix) - 1) != 0) return false;
  const char* name = path + sizeof(kPrefix) - 1;
  return name[0] != '\0' && strstr(name, "..") == nullptr &&
         strchr(name, '/') == nullptr;
}

bool removeTree(const char* path, String& error, uint8_t depth = 0) {
  if (depth > 3) {
    error = "Ride folder nesting is invalid";
    return false;
  }

  // Reopen the directory for every first child. Mutating a FAT directory while
  // advancing a live iterator can otherwise skip the entry that follows the
  // one just removed.
  while (true) {
    File directory = SD.open(path);
    if (!directory || !directory.isDirectory()) {
      if (directory) directory.close();
      error = "Ride folder not found";
      return false;
    }
    File entry = directory.openNextFile();
    if (!entry) {
      directory.close();
      break;
    }
    char childPath[128] = {0};
    strlcpy(childPath, entry.path(), sizeof(childPath));
    const bool childIsDirectory = entry.isDirectory();
    entry.close();
    directory.close();
    if (childPath[0] == '\0') {
      error = "Invalid ride file path";
      return false;
    }
    if (childIsDirectory) {
      if (!removeTree(childPath, error, depth + 1)) {
        return false;
      }
    } else if (!SD.remove(childPath)) {
      error = "Cannot delete " + String(childPath);
      return false;
    }
  }

  if (!SD.rmdir(path) || SD.exists(path)) {
    error = "Cannot remove ride folder";
    return false;
  }
  return true;
}

}  // namespace

uint8_t RideRepository::list(StorageManager& storage, RideSummaryItem* items,
                             uint8_t capacity, String& error) {
  error = String();
  if (!storage.loggingEnabled()) {
    error = "SD unavailable";
    return 0;
  }

  Guard guard;
  File directory = SD.open("/rides");
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    return 0;
  }

  uint8_t count = 0;
  for (File folder = directory.openNextFile(); folder && count < capacity;
       folder = directory.openNextFile()) {
    if (!folder.isDirectory()) {
      folder.close();
      continue;
    }

    RideSummaryItem& item = items[count];
    item = RideSummaryItem();
    strlcpy(item.folder, folder.path(), sizeof(item.folder));
    const char* underscore = strrchr(item.folder, '_');
    item.id = underscore ? strtoul(underscore + 1, nullptr, 10) : 0;

    char summaryPath[96] = {0};
    snprintf(summaryPath, sizeof(summaryPath), "%s/summary.json",
             item.folder);
    folder.close();

    File summary = SD.open(summaryPath, FILE_READ);
    if (summary) {
      StaticJsonDocument<896> document;
      if (!deserializeJson(document, summary)) {
        item.complete = true;
        item.formatVersion = document["format_version"] | 1;
        item.distanceM = document["distance_m"] | 0.0f;
        item.avgKmh = document["average_moving_speed_kmh"] | 0.0f;
        item.maxKmh = document["max_speed_kmh"] | 0.0f;
        item.elapsedMs = document["elapsed_time_ms"] | 0ULL;
        item.movingMs = document["moving_time_ms"] | 0ULL;
        item.batteryStart = document["battery_start_voltage"] | 0.0f;
        item.batteryEnd = document["battery_end_voltage"] | 0.0f;
        item.startedAtUtcMs = document["started_at_utc_ms"] | -1LL;
        item.finishedAtUtcMs = document["finished_at_utc_ms"] | -1LL;
      }
      summary.close();
    }
    ++count;
  }
  directory.close();

  // FAT directory enumeration order is not a user-facing order. Keep newest
  // ride IDs first so vertical History scrolling remains deterministic.
  for (uint8_t i = 1; i < count; ++i) {
    RideSummaryItem value = items[i];
    int16_t j = i - 1;
    while (j >= 0 && items[j].id < value.id) {
      items[j + 1] = items[j];
      --j;
    }
    items[j + 1] = value;
  }
  return count;
}

bool RideRepository::remove(StorageManager& storage,
                            const RideSummaryItem& item,
                            const RideRecoveryData& active, String& error) {
  error = String();
  if (active.valid && strcmp(active.rideFolder, item.folder) == 0) {
    error = "Cannot delete active ride";
    return false;
  }
  if (!storage.loggingEnabled()) {
    error = "SD unavailable";
    return false;
  }
  if (!isEnumeratedRideFolder(item.folder)) {
    error = "Invalid ride folder";
    return false;
  }

  Guard guard;
  return removeTree(item.folder, error);
}
