package com.diybikecomputer.companion.location

enum class LocationPermissionAction {
    RequestForeground,
    RequestBackgroundPermission,
    OpenAppSettings,
    Enable,
}

/** Pure API-level policy so the permission sequence stays testable. */
object LocationPermissionPolicy {
    fun nextAction(
        apiLevel: Int,
        preciseGranted: Boolean,
        backgroundGranted: Boolean,
    ): LocationPermissionAction = when {
        !preciseGranted -> LocationPermissionAction.RequestForeground
        apiLevel < 29 || backgroundGranted -> LocationPermissionAction.Enable
        apiLevel == 29 -> LocationPermissionAction.RequestBackgroundPermission
        else -> LocationPermissionAction.OpenAppSettings
    }
}
