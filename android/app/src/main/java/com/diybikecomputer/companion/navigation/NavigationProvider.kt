package com.diybikecomputer.companion.navigation

import kotlinx.coroutines.flow.StateFlow

enum class NavigationLifecycle(val wireValue: Int) {
    INACTIVE(0),
    STARTING(1),
    NAVIGATING(2),
    REROUTING(3),
    ARRIVED(4),
    ERROR(5),
}

enum class Maneuver(val wireValue: Int) {
    STRAIGHT(0),
    TURN_LEFT(1),
    TURN_RIGHT(2),
    SLIGHT_LEFT(3),
    SLIGHT_RIGHT(4),
    SHARP_LEFT(5),
    SHARP_RIGHT(6),
    UTURN(7),
    ROUNDABOUT(8),
    ROUNDABOUT_EXIT(9),
    DESTINATION(10),
    UNKNOWN(255),
}

data class NavigationSnapshot(
    val available: Boolean = false,
    val lifecycle: NavigationLifecycle = NavigationLifecycle.INACTIVE,
    val maneuver: Maneuver = Maneuver.UNKNOWN,
    val distanceToManeuverM: Long = 0,
    val streetName: String = "",
    val nextManeuver: Maneuver = Maneuver.UNKNOWN,
    val nextDistanceM: Long = 0,
    val remainingDistanceM: Long = 0,
    val etaUtcMs: Long = 0,
)

interface NavigationProvider {
    val state: StateFlow<NavigationSnapshot>
    val available: Boolean
}
