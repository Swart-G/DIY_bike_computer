#pragma once

#include <Arduino.h>
#include <SD.h>

#include "phone/BikeProtocol.h"
#include "storage/RideRepository.h"

class PhoneLinkManager;
class StorageManager;

enum class RideSyncState : uint8_t {
  Idle,
  BuildingManifests,
  WaitingRequest,
  SendingFileBegin,
  SendingChunk,
  WaitingAck,
  Error,
};

class RideSyncManager {
 public:
  void begin(StorageManager& storage, RideRepository& repository);
  void setRuntimeState(uint8_t rideState, bool usbActive);
  bool handleFrame(const bikeproto::Frame& frame, PhoneLinkManager& phone);
  void update(PhoneLinkManager& phone, uint32_t nowMs);
  void cancel();

  RideSyncState state() const { return state_; }
  uint32_t lastCompletedMs() const { return lastCompletedMs_; }

 private:
  struct FileManifest {
    uint8_t id = 0;
    uint32_t size = 0;
    uint32_t crc32 = 0;
  };
  struct RideManifest {
    RideSummaryItem ride;
    FileManifest files[4];
    uint8_t fileCount = 0;
    uint32_t totalSize = 0;
    uint32_t revision = 0;
  };

  static constexpr uint8_t kMaximumRides = 24;
  static constexpr uint16_t kCrcReadBytes = 512;
  static constexpr uint16_t kMaximumChunkBytes = 220;
  static constexpr uint32_t kAckTimeoutMs = 5000;

  bool available(const PhoneLinkManager& phone) const;
  bool beginListing(PhoneLinkManager& phone, uint16_t requestSequence);
  void buildManifestStep(PhoneLinkManager& phone);
  bool openNextManifestFile();
  void finishManifestFile();
  bool sendCurrentManifest(PhoneLinkManager& phone);
  bool sendListEnd(PhoneLinkManager& phone);
  void sendFileBegin(PhoneLinkManager& phone);
  bool beginDownload(const bikeproto::Frame& frame, PhoneLinkManager& phone);
  bool handleAck(const bikeproto::Frame& frame, PhoneLinkManager& phone);
  void sendChunk(PhoneLinkManager& phone, bool reread);
  void sendFileEnd(PhoneLinkManager& phone);
  RideManifest* findRide(uint32_t rideId);
  FileManifest* findFile(RideManifest& ride, uint8_t fileId);
  bool buildFilePath(const RideManifest& ride, uint8_t fileId,
                     char* path, size_t capacity) const;
  void protocolError(PhoneLinkManager& phone, bikeproto::ErrorCode code,
                     const bikeproto::Frame& frame, const char* detail);
  static uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t length);
  static const char* fileName(uint8_t fileId);

  StorageManager* storage_ = nullptr;
  RideRepository* repository_ = nullptr;
  RideSyncState state_ = RideSyncState::Idle;
  // Listing can recurse into FatFs and ArduinoJson. A 24-item local array
  // exhausts the 8 KiB Arduino loopTask stack during the initial phone sync.
  RideSummaryItem listScratch_[kMaximumRides];
  RideManifest manifests_[kMaximumRides];
  uint8_t manifestCount_ = 0;
  uint8_t buildRideIndex_ = 0;
  uint8_t buildFileId_ = 1;
  uint16_t listRequestSequence_ = 0;
  uint16_t listedCount_ = 0;
  uint32_t globalRevision_ = 0;
  File crcFile_;
  uint32_t crcAccumulator_ = 0xFFFFFFFFUL;
  uint32_t crcSize_ = 0;

  uint8_t rideState_ = 0;
  bool usbActive_ = false;
  File transferFile_;
  RideManifest* transferRide_ = nullptr;
  FileManifest* transferManifest_ = nullptr;
  uint32_t transferOffset_ = 0;
  uint16_t transferChunkSequence_ = 0;
  uint16_t transferChunkLength_ = 0;
  uint16_t transferChunkMaximum_ = 0;
  uint8_t transferChunk_[kMaximumChunkBytes] = {0};
  uint32_t ackDeadlineMs_ = 0;
  uint32_t lastCompletedMs_ = 0;
  uint16_t transferRequestSequence_ = 0;
};
