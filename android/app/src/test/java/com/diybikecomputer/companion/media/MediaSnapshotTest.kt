package com.diybikecomputer.companion.media

import org.junit.Assert.assertEquals
import org.junit.Test

class MediaSnapshotTest {
    @Test
    fun futureUpdateTimestampNeverMovesPlaybackBackwards() {
        val snapshot = MediaSnapshot(
            playing = true,
            durationMs = 20_000,
            positionMs = 5_000,
            updateRealtimeMs = 10_000,
            playbackSpeed = 1f,
        )

        assertEquals(5_000, snapshot.currentPositionMs(nowRealtimeMs = 9_000))
    }

    @Test
    fun invalidOrExcessivePlaybackEstimateStaysWithinDuration() {
        val invalidSpeed = MediaSnapshot(
            playing = true,
            durationMs = 20_000,
            positionMs = 5_000,
            updateRealtimeMs = 1_000,
            playbackSpeed = Float.NaN,
        )
        val excessiveEstimate = invalidSpeed.copy(
            playbackSpeed = Float.MAX_VALUE,
        )

        assertEquals(5_000, invalidSpeed.currentPositionMs(nowRealtimeMs = 2_000))
        assertEquals(20_000, excessiveEstimate.currentPositionMs(nowRealtimeMs = Long.MAX_VALUE))
    }
}
