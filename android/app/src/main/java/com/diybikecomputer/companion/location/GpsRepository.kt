package com.diybikecomputer.companion.location

import android.content.Context

class GpsRepository(context: Context) {
    private val preferences =
        context.getSharedPreferences("gps_assist", Context.MODE_PRIVATE)

    fun enabled(): Boolean = preferences.getBoolean("enabled", false)

    fun setEnabled(enabled: Boolean) {
        preferences.edit().putBoolean("enabled", enabled).apply()
    }

    fun recordingStatus(): String =
        preferences.getString("recording_status", "Off").orEmpty()

    fun setRecordingStatus(status: String) {
        preferences.edit().putString("recording_status", status).apply()
    }
}
