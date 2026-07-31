package com.diybikecomputer.companion.location

import org.junit.Assert.assertEquals
import org.junit.Test

class LocationPermissionPolicyTest {
    @Test
    fun `foreground permission is always requested first`() {
        assertEquals(
            LocationPermissionAction.RequestForeground,
            LocationPermissionPolicy.nextAction(36, false, false),
        )
    }

    @Test
    fun `android 10 uses the background runtime dialog`() {
        assertEquals(
            LocationPermissionAction.RequestBackgroundPermission,
            LocationPermissionPolicy.nextAction(29, true, false),
        )
    }

    @Test
    fun `android 11 and newer direct the user to app settings`() {
        assertEquals(
            LocationPermissionAction.OpenAppSettings,
            LocationPermissionPolicy.nextAction(30, true, false),
        )
        assertEquals(
            LocationPermissionAction.OpenAppSettings,
            LocationPermissionPolicy.nextAction(36, true, false),
        )
    }

    @Test
    fun `complete grants enable recording on every supported version`() {
        assertEquals(
            LocationPermissionAction.Enable,
            LocationPermissionPolicy.nextAction(26, true, false),
        )
        assertEquals(
            LocationPermissionAction.Enable,
            LocationPermissionPolicy.nextAction(36, true, true),
        )
    }
}
