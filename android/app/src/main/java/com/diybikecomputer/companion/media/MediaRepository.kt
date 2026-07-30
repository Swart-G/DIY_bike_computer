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
        val safeDuration = durationMs.coerceAtLeast(0)
        if (!playing || !playbackSpeed.isFinite() || playbackSpeed <= 0f) {
            return positionMs.coerceIn(0, safeDuration)
        }
        val elapsedMs = (nowRealtimeMs - updateRealtimeMs).coerceAtLeast(0)
        val advance = (elapsedMs.toDouble() * playbackSpeed)
            .coerceIn(0.0, Long.MAX_VALUE.toDouble())
            .toLong()
        val safePosition = positionMs.coerceAtLeast(0)
        val estimate =
            if (safePosition > Long.MAX_VALUE - advance) Long.MAX_VALUE
            else safePosition + advance
        return estimate.coerceIn(0, safeDuration)
    }
}

data class MediaPlayerOption(
    val packageName: String,
    val name: String,
    val playing: Boolean,
    val title: String,
)

class MediaRepository(private val context: Context) {
    private val preferences =
        context.getSharedPreferences("media_control", Context.MODE_PRIVATE)
    private val mutableState = MutableStateFlow(MediaSnapshot())
    val state: StateFlow<MediaSnapshot> = mutableState.asStateFlow()
    private val mutablePlayers = MutableStateFlow<List<MediaPlayerOption>>(emptyList())
    val players: StateFlow<List<MediaPlayerOption>> = mutablePlayers.asStateFlow()
    private val mutablePreferredPackage =
        MutableStateFlow(preferences.getString("preferred_package", null))
    val preferredPackage: StateFlow<String?> = mutablePreferredPackage.asStateFlow()
    private var controllers: List<MediaController> = emptyList()
    private var controller: MediaController? = null
    private val callback = object : MediaController.Callback() {
        override fun onPlaybackStateChanged(state: PlaybackState?) = refresh()
        override fun onMetadataChanged(metadata: MediaMetadata?) = refresh()
        override fun onSessionDestroyed() {
            controllers = controllers.filterNot { it.sessionToken == controller?.sessionToken }
            selectController()
        }
    }

    @Synchronized
    fun updateControllers(next: List<MediaController>) {
        controllers = next
        selectController()
    }

    @Synchronized
    fun setPreferredPlayer(packageName: String?) {
        mutablePreferredPackage.value = packageName
        preferences.edit().apply {
            if (packageName == null) remove("preferred_package")
            else putString("preferred_package", packageName)
        }.apply()
        selectController()
    }

    @Synchronized
    private fun selectController() {
        val preferred = mutablePreferredPackage.value
        val next = if (preferred != null) {
            controllers.firstOrNull {
                it.packageName == preferred && isPlaying(it.playbackState)
            } ?: controllers.firstOrNull { it.packageName == preferred }
        } else {
            controllers.firstOrNull { isPlaying(it.playbackState) }
                ?: controllers.firstOrNull()
        }
        bind(next)
    }

    @Synchronized
    private fun bind(next: MediaController?) {
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
        mutablePlayers.value = controllers
            .groupBy(MediaController::getPackageName)
            .map { (packageName, packageControllers) ->
                val representative = packageControllers.firstOrNull {
                    isPlaying(it.playbackState)
                } ?: packageControllers.first()
                MediaPlayerOption(
                    packageName = packageName,
                    name = playerName(representative),
                    playing = packageControllers.any { isPlaying(it.playbackState) },
                    title = representative.metadata
                        ?.getString(MediaMetadata.METADATA_KEY_TITLE).orEmpty(),
                )
            }
            .sortedWith(compareByDescending<MediaPlayerOption> { it.playing }.thenBy { it.name })
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
            player = playerName(active),
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

    private fun playerName(controller: MediaController): String = runCatching {
        context.packageManager.getApplicationLabel(
            context.packageManager.getApplicationInfo(controller.packageName, 0),
        ).toString()
    }.getOrDefault(controller.packageName)
}
