#include "usb/UsbMassStorageManager.h"

#if defined(ARDUINO_ARCH_ESP32) && __has_include(<USB.h>) && __has_include(<USBMSC.h>) && \
    __has_include(<diskio.h>)
#include <USB.h>
#include <USBMSC.h>
extern "C" {
#include <ff.h>
#include <diskio.h>
}
#define BIKE_USB_MSC_AVAILABLE 1
#else
#define BIKE_USB_MSC_AVAILABLE 0
#endif

#include <SD.h>

namespace {

#if BIKE_USB_MSC_AVAILABLE
USBMSC g_msc;
static constexpr uint8_t kSdPdrv = 0;
static constexpr uint32_t kSectorSize = 512;

int32_t mscReadCallback(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  uint8_t* out = static_cast<uint8_t*>(buffer);
  uint32_t remaining = bufsize;
  uint32_t sector = lba;
  uint32_t sectorOffset = offset;
  uint8_t scratch[kSectorSize];

  while (remaining > 0) {
    const uint32_t chunk = min(kSectorSize - sectorOffset, remaining);
    if (sectorOffset == 0 && chunk == kSectorSize) {
      if (disk_read(kSdPdrv, out, sector, 1) != RES_OK) {
        return -1;
      }
    } else {
      if (disk_read(kSdPdrv, scratch, sector, 1) != RES_OK) {
        return -1;
      }
      memcpy(out, scratch + sectorOffset, chunk);
    }
    out += chunk;
    remaining -= chunk;
    ++sector;
    sectorOffset = 0;
  }
  return static_cast<int32_t>(bufsize);
}

int32_t mscWriteCallback(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  uint8_t* in = buffer;
  uint32_t remaining = bufsize;
  uint32_t sector = lba;
  uint32_t sectorOffset = offset;
  uint8_t scratch[kSectorSize];

  while (remaining > 0) {
    const uint32_t chunk = min(kSectorSize - sectorOffset, remaining);
    if (sectorOffset == 0 && chunk == kSectorSize) {
      if (disk_write(kSdPdrv, in, sector, 1) != RES_OK) {
        return -1;
      }
    } else {
      if (disk_read(kSdPdrv, scratch, sector, 1) != RES_OK) {
        return -1;
      }
      memcpy(scratch + sectorOffset, in, chunk);
      if (disk_write(kSdPdrv, scratch, sector, 1) != RES_OK) {
        return -1;
      }
    }
    in += chunk;
    remaining -= chunk;
    ++sector;
    sectorOffset = 0;
  }
  return static_cast<int32_t>(bufsize);
}

bool mscStartStopCallback(uint8_t powerCondition, bool start, bool loadEject) {
  (void)powerCondition;
  (void)start;
  (void)loadEject;
  return true;
}
#endif

}  // namespace

bool UsbMassStorageManager::begin(StorageManager& storage) {
  if (active_) {
    status_ = "USB Mass Storage already active";
    return true;
  }
  if (!storage.sdAvailable()) {
    status_ = "USB MSC error: SD card not found";
    return false;
  }

#if BIKE_USB_MSC_AVAILABLE
  const uint64_t cardBytes = SD.cardSize();
  if (cardBytes < kSectorSize) {
    status_ = "USB MSC error: invalid SD size";
    return false;
  }

  storage.setUsbModeActive(true);

  g_msc.vendorID("BikeSPD");
  g_msc.productID("ESP32S3 SD");
  g_msc.productRevision("0.1");
  g_msc.onRead(mscReadCallback);
  g_msc.onWrite(mscWriteCallback);
  g_msc.onStartStop(mscStartStopCallback);
  g_msc.mediaPresent(true);

  const uint32_t sectorCount = static_cast<uint32_t>(cardBytes / kSectorSize);
  if (!g_msc.begin(sectorCount, kSectorSize)) {
    storage.setUsbModeActive(false);
    status_ = "USB MSC begin failed";
    return false;
  }
  USB.begin();
  active_ = true;
  status_ = "USB Mass Storage active";
  return true;
#else
  status_ = "USB MSC not compiled in for this Arduino-ESP32 core";
  return false;
#endif
}
