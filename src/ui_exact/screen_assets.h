#pragma once
#include <Arduino.h>

namespace ui_exact {
constexpr uint16_t kScreenWidth = 480;
constexpr uint16_t kScreenHeight = 320;
struct ScreenAsset { const char* name; const uint32_t* runs; uint32_t runCount; };
enum class ScreenId : uint16_t {
  SCREEN_00_BOOT,
  SCREEN_01_SD_MISSING,
  SCREEN_02_RECOVERY,
  SCREEN_03_HOME_DISCONNECTED,
  SCREEN_04_HOME_CONNECTED,
  SCREEN_10_RIDE_IDLE,
  SCREEN_11_RIDE_SPEED_ACTIVE,
  SCREEN_12_RIDE_SPEED_PAUSED,
  SCREEN_13_RIDE_STATS,
  SCREEN_14_RIDE_GRAPH,
  SCREEN_15_RIDE_NAVIGATION,
  SCREEN_16_RIDE_MEDIA,
  SCREEN_17_RIDE_FINISH_CONFIRM,
  SCREEN_18_RIDE_SUMMARY,
  SCREEN_19_RAIN_LOCK_ENABLED,
  SCREEN_20_RAIN_UNLOCK_HOLD_1,
  SCREEN_21_RAIN_UNLOCK_HOLD_2,
  SCREEN_22_RAIN_UNLOCK_HOLD_3,
  SCREEN_23_RAIN_UNLOCK_SUCCESS,
  SCREEN_30_PHONE_UNPAIRED,
  SCREEN_31_PHONE_PAIRING,
  SCREEN_32_PHONE_CONNECTED,
  SCREEN_33_PHONE_SYNCING,
  SCREEN_34_PHONE_SYNC_COMPLETE,
  SCREEN_35_PHONE_LOST,
  SCREEN_40_HISTORY_EMPTY,
  SCREEN_41_HISTORY_LIST,
  SCREEN_42_HISTORY_DETAIL,
  SCREEN_43_HISTORY_DELETE_CONFIRM,
  SCREEN_50_NAV_WAITING,
  SCREEN_51_NAV_ACTIVE,
  SCREEN_52_NAV_REROUTING,
  SCREEN_53_NAV_ARRIVED,
  SCREEN_54_NAV_PHONE_LOST,
  SCREEN_60_MEDIA_NO_SESSION,
  SCREEN_61_MEDIA_PLAYING,
  SCREEN_62_MEDIA_PAUSED,
  SCREEN_70_SETTINGS_MAIN,
  SCREEN_71_SETTINGS_RIDE,
  SCREEN_72_SETTINGS_DISPLAY,
  SCREEN_73_SETTINGS_PHONE,
  SCREEN_74_SETTINGS_SYSTEM,
  SCREEN_75_SETTINGS_WHEEL,
  SCREEN_76_SETTINGS_STOP_THRESHOLD,
  SCREEN_80_DIAGNOSTICS,
  SCREEN_81_DISPLAY_TEST,
  SCREEN_82_TOUCH_RAW,
  SCREEN_83_PAINT_TEST,
  SCREEN_84_SD_TEST,
  SCREEN_85_SENSOR_TEST,
  SCREEN_86_BATTERY_TEST,
  SCREEN_87_SYSTEM_INFO,
  SCREEN_88_USB_STORAGE,
  SCREEN_90_ABOUT,
  SCREEN_91_DIALOG_FORGET_PHONE,
  SCREEN_92_DIALOG_RESET_SETTINGS,
  SCREEN_93_DIALOG_SD_ERROR,
  SCREEN_94_DIALOG_PROTOCOL_MISMATCH,
  SCREEN_95_OVERLAY_LOW_BATTERY,
  SCREEN_96_OVERLAY_SYNC_COMPLETE,
  COUNT
};
extern const ScreenAsset kAsset_00_boot;
extern const ScreenAsset kAsset_01_sd_missing;
extern const ScreenAsset kAsset_02_recovery;
extern const ScreenAsset kAsset_03_home_disconnected;
extern const ScreenAsset kAsset_04_home_connected;
extern const ScreenAsset kAsset_10_ride_idle;
extern const ScreenAsset kAsset_11_ride_speed_active;
extern const ScreenAsset kAsset_12_ride_speed_paused;
extern const ScreenAsset kAsset_13_ride_stats;
extern const ScreenAsset kAsset_14_ride_graph;
extern const ScreenAsset kAsset_15_ride_navigation;
extern const ScreenAsset kAsset_16_ride_media;
extern const ScreenAsset kAsset_17_ride_finish_confirm;
extern const ScreenAsset kAsset_18_ride_summary;
extern const ScreenAsset kAsset_19_rain_lock_enabled;
extern const ScreenAsset kAsset_20_rain_unlock_hold_1;
extern const ScreenAsset kAsset_21_rain_unlock_hold_2;
extern const ScreenAsset kAsset_22_rain_unlock_hold_3;
extern const ScreenAsset kAsset_23_rain_unlock_success;
extern const ScreenAsset kAsset_30_phone_unpaired;
extern const ScreenAsset kAsset_31_phone_pairing;
extern const ScreenAsset kAsset_32_phone_connected;
extern const ScreenAsset kAsset_33_phone_syncing;
extern const ScreenAsset kAsset_34_phone_sync_complete;
extern const ScreenAsset kAsset_35_phone_lost;
extern const ScreenAsset kAsset_40_history_empty;
extern const ScreenAsset kAsset_41_history_list;
extern const ScreenAsset kAsset_42_history_detail;
extern const ScreenAsset kAsset_43_history_delete_confirm;
extern const ScreenAsset kAsset_50_nav_waiting;
extern const ScreenAsset kAsset_51_nav_active;
extern const ScreenAsset kAsset_52_nav_rerouting;
extern const ScreenAsset kAsset_53_nav_arrived;
extern const ScreenAsset kAsset_54_nav_phone_lost;
extern const ScreenAsset kAsset_60_media_no_session;
extern const ScreenAsset kAsset_61_media_playing;
extern const ScreenAsset kAsset_62_media_paused;
extern const ScreenAsset kAsset_70_settings_main;
extern const ScreenAsset kAsset_71_settings_ride;
extern const ScreenAsset kAsset_72_settings_display;
extern const ScreenAsset kAsset_73_settings_phone;
extern const ScreenAsset kAsset_74_settings_system;
extern const ScreenAsset kAsset_75_settings_wheel;
extern const ScreenAsset kAsset_76_settings_stop_threshold;
extern const ScreenAsset kAsset_80_diagnostics;
extern const ScreenAsset kAsset_81_display_test;
extern const ScreenAsset kAsset_82_touch_raw;
extern const ScreenAsset kAsset_83_paint_test;
extern const ScreenAsset kAsset_84_sd_test;
extern const ScreenAsset kAsset_85_sensor_test;
extern const ScreenAsset kAsset_86_battery_test;
extern const ScreenAsset kAsset_87_system_info;
extern const ScreenAsset kAsset_88_usb_storage;
extern const ScreenAsset kAsset_90_about;
extern const ScreenAsset kAsset_91_dialog_forget_phone;
extern const ScreenAsset kAsset_92_dialog_reset_settings;
extern const ScreenAsset kAsset_93_dialog_sd_error;
extern const ScreenAsset kAsset_94_dialog_protocol_mismatch;
extern const ScreenAsset kAsset_95_overlay_low_battery;
extern const ScreenAsset kAsset_96_overlay_sync_complete;
const ScreenAsset& getScreenAsset(ScreenId id);
} // namespace ui_exact
