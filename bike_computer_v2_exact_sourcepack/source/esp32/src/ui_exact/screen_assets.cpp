#include "ui_exact/screen_assets.h"
namespace ui_exact {
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
static const ScreenAsset* const kAssets[] = {
  &kAsset_00_boot,
  &kAsset_01_sd_missing,
  &kAsset_02_recovery,
  &kAsset_03_home_disconnected,
  &kAsset_04_home_connected,
  &kAsset_10_ride_idle,
  &kAsset_11_ride_speed_active,
  &kAsset_12_ride_speed_paused,
  &kAsset_13_ride_stats,
  &kAsset_14_ride_graph,
  &kAsset_15_ride_navigation,
  &kAsset_16_ride_media,
  &kAsset_17_ride_finish_confirm,
  &kAsset_18_ride_summary,
  &kAsset_19_rain_lock_enabled,
  &kAsset_20_rain_unlock_hold_1,
  &kAsset_21_rain_unlock_hold_2,
  &kAsset_22_rain_unlock_hold_3,
  &kAsset_23_rain_unlock_success,
  &kAsset_30_phone_unpaired,
  &kAsset_31_phone_pairing,
  &kAsset_32_phone_connected,
  &kAsset_33_phone_syncing,
  &kAsset_34_phone_sync_complete,
  &kAsset_35_phone_lost,
  &kAsset_40_history_empty,
  &kAsset_41_history_list,
  &kAsset_42_history_detail,
  &kAsset_43_history_delete_confirm,
  &kAsset_50_nav_waiting,
  &kAsset_51_nav_active,
  &kAsset_52_nav_rerouting,
  &kAsset_53_nav_arrived,
  &kAsset_54_nav_phone_lost,
  &kAsset_60_media_no_session,
  &kAsset_61_media_playing,
  &kAsset_62_media_paused,
  &kAsset_70_settings_main,
  &kAsset_71_settings_ride,
  &kAsset_72_settings_display,
  &kAsset_73_settings_phone,
  &kAsset_74_settings_system,
  &kAsset_75_settings_wheel,
  &kAsset_76_settings_stop_threshold,
  &kAsset_80_diagnostics,
  &kAsset_81_display_test,
  &kAsset_82_touch_raw,
  &kAsset_83_paint_test,
  &kAsset_84_sd_test,
  &kAsset_85_sensor_test,
  &kAsset_86_battery_test,
  &kAsset_87_system_info,
  &kAsset_88_usb_storage,
  &kAsset_90_about,
  &kAsset_91_dialog_forget_phone,
  &kAsset_92_dialog_reset_settings,
  &kAsset_93_dialog_sd_error,
  &kAsset_94_dialog_protocol_mismatch,
  &kAsset_95_overlay_low_battery,
  &kAsset_96_overlay_sync_complete
};
const ScreenAsset& getScreenAsset(ScreenId id) {
  uint16_t i=static_cast<uint16_t>(id);
  if(i>=static_cast<uint16_t>(ScreenId::COUNT)) i=0;
  return *kAssets[i];
}
} // namespace ui_exact
