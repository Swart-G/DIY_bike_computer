package com.diybikecomputer.companion.location

import android.content.Context

class GpsRepository(context: Context) {
    private val preferences =
        context.getSharedPreferences("gps_assist", Context.MODE_PRIVATE)

    fun enabled(): Boolean = preferences.getBoolean("enabled", false)

    fun setEnabled(enabled: Boolean) {
        preferences.edit().putBoolean("enabled", enabled).apply()
    }
}
