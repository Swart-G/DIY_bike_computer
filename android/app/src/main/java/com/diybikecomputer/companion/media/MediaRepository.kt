package com.diybikecomputer.companion.media

import android.content.Context
import android.media.MediaMetadata
import android.media.session.MediaController
import android.media.session.PlaybackState
import android.os.SystemClock
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

object MediaActionMask {
    const val PLAY = 1 shl 0
    const val PAUSE = 1 shl 1
    const val TOGGLE = 1 shl 2
    const val NEXT = 1 shl 3
    const val PREVIOUS = 1 shl 4
    const val SEEK = 1 shl 5
}

object MediaAction {
    const val PLAY = 0
    const val PAUSE = 1
    const val TOGGLE = 2
    const val NEXT = 3
    const val PREVIOUS = 4
    const val SEEK = 5
}

data class MediaSnapshot(
    val available: Boolean = false,
    val playing: Boolean = false,
    val supportedActions: Int = 0,
    val durationMs: Long = 0,
    val positionMs: Long = 0,
    val updateRealtimeMs: Long = 0,
    val playbackSpeed: Float = 0f,
    val player: String = "",
    val title: String = "",
    val artist: String = "",
) {
    fun currentPositionMs(nowRealtimeMs: Long = SystemClock.elapsedRealtime()): Long {
        if (!playing || playbackSpeed <= 0f) return positionMs.coerceIn(0, durationMs.coerceAtLeast(0))
        val estimate = positionMs + ((nowRealtimeMs - updateRealtimeMs) * playbackSpeed).toLong()
        return estimate.coerceIn(0, durationMs.coerceAtLeast(0))
    }
}

class MediaRepository(private val context: Context) {
    private val mutableState = MutableStateFlow(MediaSnapshot())
    val state: StateFlow<MediaSnapshot> = mutableState.asStateFlow()
    private var controller: MediaController? = null
    private val callback = object : MediaController.Callback() {
        override fun onPlaybackStateChanged(state: PlaybackState?) = refresh()
        override fun onMetadataChanged(metadata: MediaMetadata?) = refresh()
        override fun onSessionDestroyed() = bind(null)
    }

    @Synchronized
    fun bind(next: MediaController?) {
        if (controller?.sessionToken == next?.sessionToken) {
            refresh()
            return
        }
        controller?.unregisterCallback(callback)
        controller = next
        controller?.registerCallback(callback)
        refresh()
    }

    @Synchronized
    fun perform(action: Int, positionMs: Long) {
        val active = controller ?: return
        val controls = active.transportControls
        when (action) {
            MediaAction.PLAY -> controls.play()
            MediaAction.PAUSE -> controls.pause()
            MediaAction.TOGGLE -> {
                if (isPlaying(active.playbackState)) controls.pause() else controls.play()
            }
            MediaAction.NEXT -> controls.skipToNext()
            MediaAction.PREVIOUS -> controls.skipToPrevious()
            MediaAction.SEEK -> controls.seekTo(positionMs.coerceAtLeast(0))
        }
    }

    @Synchronized
    private fun refresh() {
        val active = controller
        if (active == null) {
            mutableState.value = MediaSnapshot()
            return
        }
        val metadata = active.metadata
        val playback = active.playbackState
        val actions = playback?.actions ?: 0L
        mutableState.value = MediaSnapshot(
            available = true,
            playing = isPlaying(playback),
            supportedActions =
                (if ((actions and PlaybackState.ACTION_PLAY) != 0L) MediaActionMask.PLAY else 0) or
                    (if ((actions and PlaybackState.ACTION_PAUSE) != 0L) MediaActionMask.PAUSE else 0) or
                    (if ((actions and (PlaybackState.ACTION_PLAY_PAUSE or
                            PlaybackState.ACTION_PLAY or PlaybackState.ACTION_PAUSE)) != 0L
                    ) {
                        MediaActionMask.TOGGLE
                    } else {
                        0
                    }) or
                    (if ((actions and PlaybackState.ACTION_SKIP_TO_NEXT) != 0L) MediaActionMask.NEXT else 0) or
                    (if ((actions and PlaybackState.ACTION_SKIP_TO_PREVIOUS) != 0L) MediaActionMask.PREVIOUS else 0) or
                    (if ((actions and PlaybackState.ACTION_SEEK_TO) != 0L) MediaActionMask.SEEK else 0),
            durationMs = metadata?.getLong(MediaMetadata.METADATA_KEY_DURATION)?.coerceAtLeast(0) ?: 0,
            positionMs = playback?.position?.coerceAtLeast(0) ?: 0,
            updateRealtimeMs = playback?.lastPositionUpdateTime ?: SystemClock.elapsedRealtime(),
            playbackSpeed = playback?.playbackSpeed ?: 0f,
            player = runCatching {
                context.packageManager.getApplicationLabel(
                    context.packageManager.getApplicationInfo(active.packageName, 0),
                ).toString()
            }.getOrDefault(active.packageName),
            title = metadata?.getString(MediaMetadata.METADATA_KEY_TITLE)
                ?: metadata?.getString(MediaMetadata.METADATA_KEY_DISPLAY_TITLE).orEmpty(),
            artist = metadata?.getString(MediaMetadata.METADATA_KEY_ARTIST)
                ?: metadata?.getString(MediaMetadata.METADATA_KEY_DISPLAY_SUBTITLE).orEmpty(),
        )
    }

    private fun isPlaying(state: PlaybackState?): Boolean =
        state?.state == PlaybackState.STATE_PLAYING ||
            state?.state == PlaybackState.STATE_BUFFERING ||
            state?.state == PlaybackState.STATE_CONNECTING
}
