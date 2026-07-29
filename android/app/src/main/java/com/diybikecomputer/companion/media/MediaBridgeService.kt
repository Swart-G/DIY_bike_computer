package com.diybikecomputer.companion.media

import android.content.ComponentName
import android.media.session.MediaController
import android.media.session.MediaSessionManager
import android.service.notification.NotificationListenerService

class MediaBridgeService : NotificationListenerService() {
    private val repository: MediaRepository
        get() = (application as com.diybikecomputer.companion.BikeComputerApplication).mediaRepository
    private lateinit var sessions: MediaSessionManager
    private val listener = MediaSessionManager.OnActiveSessionsChangedListener(::selectSession)

    override fun onListenerConnected() {
        super.onListenerConnected()
        sessions = getSystemService(MediaSessionManager::class.java)
        val component = ComponentName(this, MediaBridgeService::class.java)
        sessions.addOnActiveSessionsChangedListener(listener, component)
        selectSession(sessions.getActiveSessions(component))
    }

    override fun onListenerDisconnected() {
        if (::sessions.isInitialized) {
            sessions.removeOnActiveSessionsChangedListener(listener)
        }
        repository.bind(null)
        super.onListenerDisconnected()
    }

    private fun selectSession(controllers: List<MediaController>?) {
        val selected = controllers.orEmpty().firstOrNull {
            it.playbackState?.state == android.media.session.PlaybackState.STATE_PLAYING
        } ?: controllers.orEmpty().firstOrNull()
        repository.bind(selected)
    }
}
