#include "storage/RideLogger.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <SD.h>

#include "bus/SharedSpiBus.h"
#include "storage/StorageManager.h"
#include "time/ClockManager.h"

namespace {

class Guard {
 public:
  Guard() : g_(true) {}

 private:
  hw::SharedSpiBusGuard g_;
};

enum class AppendResult : uint8_t {
  Ok,
  OpenFailed,
  ShortWrite,
};

AppendResult appendOnce(const char* file, const char* line) {
  Guard guard;
  File f = SD.open(file, FILE_APPEND);
  if (!f) return AppendResult::OpenFailed;
  const size_t length = strlen(line);
  const bool ok = f.print(line) == length;
  f.flush();
  f.close();
  return ok ? AppendResult::Ok : AppendResult::ShortWrite;
}

}

bool RideLogger::append(StorageManager& storage, const char* file, const char* line) {
  if (!storage.loggingEnabled()) return false;
  AppendResult result = appendOnce(file, line);
  if (result == AppendResult::Ok) return true;

  // It is safe to retry only when open failed and therefore no part of the
  // append reached the card. A short write may already contain a partial CSV
  // row, so retrying it could duplicate data.
  if (result == AppendResult::OpenFailed &&
      storage.recoverIoFailure("open write")) {
    result = appendOnce(file, line);
    if (result == AppendResult::Ok) return true;
  }

  storage.reportIoFailure(result == AppendResult::OpenFailed
                              ? "open write"
                              : "short write");
  return false;
}
bool RideLogger::start(StorageManager& storage, const app::AppSettings& settings, const BatteryMonitor& battery, String& error) {
  if (!storage.loggingEnabled()) { error = "Logging unavailable"; return false; }
  if (!storage.ensureReadyForIo("ride start preflight", error)) return false;
  Preferences prefs; prefs.begin("bike", false); rideId_ = prefs.getUInt("next_ride_id", 1); prefs.putUInt("next_ride_id", rideId_ + 1); prefs.end();
  snprintf(folder_, sizeof(folder_), "/rides/ride_%06lu", static_cast<unsigned long>(rideId_));
  if (!storage.ensureDirectory("/rides", error)) {
    if (error.length() == 0) error = "Cannot create rides directory";
    return false;
  }
  if (!storage.createDirectory(folder_, error)) {
    if (error.length() == 0) error = "Cannot create ride directory";
    return false;
  }
  sampleIndex_ = 0; sampleIntervalMs_ = settings.logSampleIntervalMs; bufferedCount_ = 0; bufferedHead_ = 0; loggingGap_ = false; active_ = true;
  rideStartedMonotonicMs_ = ClockManager::monotonicMs();
  startedAtUtcMs_ =
      clock_ && clock_->synced()
          ? clock_->epochNowMs(rideStartedMonotonicMs_)
          : 0;
  batteryStart_ = battery.voltage(); batteryMin_ = batteryStart_; batteryMax_ = batteryStart_;
  if (!writeMeta(storage, settings, error)) { active_ = false; return false; }
  char path[64]; snprintf(path, sizeof(path), "%s/samples.csv", folder_);
  if (!append(storage, path, "sample_index,ride_time_ms,state,speed_kmh,raw_speed_kmh,distance_m,avg_speed_kmh,max_speed_kmh,elapsed_time_ms,recording_time_ms,moving_time_ms,pause_time_ms,pulse_count,rejected_pulse_count,battery_voltage,battery_percent\n")) { error = "Cannot create samples"; active_ = false; return false; }
  snprintf(path, sizeof(path), "%s/events.csv", folder_);
  if (!append(storage, path, "ride_time_ms,event,details\n")) { error = "Cannot create events"; active_ = false; return false; }
  return true;
}
bool RideLogger::writeMeta(StorageManager& storage, const app::AppSettings& s, String& error) {
  StaticJsonDocument<768> d; d["format_version"] = app::RIDE_LOG_FORMAT_VERSION; d["ride_id"] = rideId_; d["firmware_version"] = app::FIRMWARE_VERSION; d["board"] = app::BOARD_NAME; d["display"] = app::DISPLAY_NAME; d["touch"] = app::TOUCH_NAME; d["wheel_circumference_m"] = s.wheelCircumferenceM; d["pulses_per_revolution"] = s.pulsesPerRevolution; d["sensor_active_level"] = app::levelToString(s.sensorActiveLevel); d["started_at"] = nullptr;
  if (startedAtUtcMs_) {
    d["started_at_utc_ms"] = startedAtUtcMs_;
    d["started_at_source"] = "android";
    d["utc_offset_seconds"] = clock_->utcOffsetSeconds();
    d["timezone_id"] = clock_->timezoneId();
  } else {
    d["started_at_utc_ms"] = nullptr;
    d["started_at_source"] = "unavailable";
  }
  char path[64]; snprintf(path, sizeof(path), "%s/meta.json", folder_); return storage.writeJsonAtomic(path, d, error);
}
bool RideLogger::resume(StorageManager& storage, const RideRecoveryData& r, String& error) {
  if (!storage.loggingEnabled() || !r.rideId || !r.rideFolder[0]) { error = "Recovery log unavailable"; return false; }
  { Guard guard; if (!SD.exists(r.rideFolder)) { error = "Ride folder missing"; return false; } }
  rideId_ = r.rideId; strncpy(folder_, r.rideFolder, sizeof(folder_)-1); folder_[sizeof(folder_)-1] = 0; sampleIndex_ = r.lastSavedSampleIndex; loggingGap_ = r.loggingGap; batteryStart_=r.batteryStartVoltage; batteryMin_=r.batteryMinVoltage; batteryMax_=r.batteryMaxVoltage; active_ = true;
  rideStartedMonotonicMs_ = 0;
  startedAtUtcMs_ = 0;
  char path[64], line[160]; snprintf(path, sizeof(path), "%s/events.csv", folder_);
  snprintf(line, sizeof(line), "%llu,RECOVERED_AS_PAUSED,\"restored after restart\"\n", static_cast<unsigned long long>(r.stats.elapsedMs));
  return append(storage, path, line);
}
bool RideLogger::logSample(StorageManager& storage, const RideStateMachine& ride, const SpeedCalculator& speed, const HallSensorSnapshot& sensor, const BatteryMonitor& battery, uint32_t nowMs) {
  if (!active_ || ride.state() == RideState::IDLE || ride.state() == RideState::FINISHED) return false;
  if (lastSampleMs_ && nowMs - lastSampleMs_ < sampleIntervalMs_) return true;
  lastSampleMs_ = nowMs; const RideStats& s = ride.stats(); char line[256];
  const float bv = battery.voltage(); if (bv > .1f) { if (!batteryMin_ || bv < batteryMin_) batteryMin_ = bv; if (bv > batteryMax_) batteryMax_ = bv; }
  snprintf(line, sizeof(line), "%lu,%llu,%s,%.2f,%.2f,%.2f,%.2f,%.2f,%llu,%llu,%llu,%llu,%lu,%lu,%.3f,%u\n", static_cast<unsigned long>(sampleIndex_++), static_cast<unsigned long long>(s.elapsedMs), ride.stateText().c_str(), speed.currentKmh(), speed.rawKmh(), s.distanceM, s.averageMovingSpeedKmh, s.maxSpeedKmh, static_cast<unsigned long long>(s.elapsedMs), static_cast<unsigned long long>(s.recordingMs), static_cast<unsigned long long>(s.movingMs), static_cast<unsigned long long>(s.pauseMs), static_cast<unsigned long>(s.pulseCount), static_cast<unsigned long>(sensor.rejectedPulseCount), bv, battery.percent());
  char path[64]; snprintf(path, sizeof(path), "%s/samples.csv", folder_);
  if (storage.loggingEnabled() && append(storage, path, line)) { flushBuffered(storage); return true; }
  loggingGap_ = true; queueSample(line); return false;
}
void RideLogger::queueSample(const char* line) { strncpy(buffered_[bufferedHead_], line, sizeof(buffered_[0])-1); buffered_[bufferedHead_][sizeof(buffered_[0])-1] = 0; bufferedHead_ = (bufferedHead_+1)%8; if (bufferedCount_ < 8) ++bufferedCount_; }
bool RideLogger::flushBuffered(StorageManager& storage) { if (!bufferedCount_) return true; char path[64]; snprintf(path,sizeof(path),"%s/samples.csv",folder_); const uint8_t first = (bufferedHead_ + 8 - bufferedCount_)%8; for(uint8_t i=0;i<bufferedCount_;++i) if(!append(storage,path,buffered_[(first+i)%8])) return false; bufferedCount_=0; return true; }
bool RideLogger::event(StorageManager& storage, const RideStateMachine& ride, const char* e, const char* details) { if (!active_ || !storage.loggingEnabled()) return false; char path[64], line[192]; snprintf(path,sizeof(path),"%s/events.csv",folder_); snprintf(line,sizeof(line),"%llu,%s,\"%s\"\n",static_cast<unsigned long long>(ride.stats().elapsedMs),e,details); return append(storage,path,line); }
bool RideLogger::writeSummary(StorageManager& storage, const RideStateMachine& ride, const BatteryMonitor& battery, String& error) { const RideStats& s=ride.stats(); StaticJsonDocument<896> d; d["format_version"]=app::RIDE_LOG_FORMAT_VERSION; d["ride_id"]=rideId_; d["state"]="FINISHED"; d["distance_m"]=s.distanceM; d["max_speed_kmh"]=s.maxSpeedKmh; d["average_moving_speed_kmh"]=s.averageMovingSpeedKmh; d["average_recording_speed_kmh"]=s.averageRecordingSpeedKmh; d["elapsed_time_ms"]=s.elapsedMs; d["recording_time_ms"]=s.recordingMs; d["moving_time_ms"]=s.movingMs; d["pause_time_ms"]=s.pauseMs; d["stopped_time_ms"]=s.stoppedMs; d["accepted_pulse_count"]=s.pulseCount; d["rejected_pulse_count"]=s.rejectedPulseCount; d["battery_start_voltage"]=batteryStart_; d["battery_end_voltage"]=battery.voltage(); d["battery_min_voltage"]=batteryMin_; d["battery_max_voltage"]=batteryMax_; d["logging_gap"]=loggingGap_; if(startedAtUtcMs_) d["started_at_utc_ms"]=startedAtUtcMs_; else d["started_at_utc_ms"]=nullptr; if(clock_&&clock_->synced()) d["finished_at_utc_ms"]=clock_->epochNowMs(ClockManager::monotonicMs()); else d["finished_at_utc_ms"]=nullptr; char path[64]; snprintf(path,sizeof(path),"%s/summary.json",folder_); return storage.writeJsonAtomic(path,d,error); }
bool RideLogger::finish(StorageManager& storage, const RideStateMachine& ride, const BatteryMonitor& battery, String& error) { if (!active_) return true; event(storage,ride,"FINISH","user finished ride"); flushBuffered(storage); if (!writeSummary(storage,ride,battery,error)) return false; active_=false; return true; }
bool RideLogger::retryPending(StorageManager& storage, const RideStateMachine& ride) { const uint32_t now=millis(); if (!active_ || !bufferedCount_ || now-lastRetryMs_<10000) return false; lastRetryMs_=now; if (!storage.sdAvailable()) storage.retry(); if (!storage.loggingEnabled()) return false; if (flushBuffered(storage)) { event(storage,ride,"SD_RESTORED","buffered samples flushed"); return true; } return false; }

bool RideLogger::applyClockSync(StorageManager& storage, String& error) {
  if (!active_ || startedAtUtcMs_ || !rideStartedMonotonicMs_ || !clock_ ||
      !clock_->synced()) {
    return true;
  }
  const uint64_t now = ClockManager::monotonicMs();
  startedAtUtcMs_ = clock_->epochNowMs(now) -
                    static_cast<int64_t>(now - rideStartedMonotonicMs_);
  char path[64];
  snprintf(path, sizeof(path), "%s/meta.json", folder_);
  StaticJsonDocument<896> document;
  if (!storage.readJson(path, document, error)) {
    startedAtUtcMs_ = 0;
    return false;
  }
  document["started_at_utc_ms"] = startedAtUtcMs_;
  document["started_at_source"] = "android";
  document["utc_offset_seconds"] = clock_->utcOffsetSeconds();
  document["timezone_id"] = clock_->timezoneId();
  if (!storage.writeJsonAtomic(path, document, error)) {
    startedAtUtcMs_ = 0;
    return false;
  }
  return true;
}
