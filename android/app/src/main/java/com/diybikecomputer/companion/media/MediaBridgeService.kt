package com.diybikecomputer.companion.media

import android.content.ComponentName
import android.media.session.MediaController
import android.media.session.MediaSessionManager
import android.service.notification.NotificationListenerService

class MediaBridgeService : NotificationListenerService() {
    private val repository: MediaRepository
        get() = (application as com.diybikecomputer.companion.BikeComputerApplication).mediaRepository
    private lateinit var sessions: MediaSessionManager
    private val listener = MediaSessionManager.OnActiveSessionsChangedListener(::updateSessions)

    override fun onListenerConnected() {
        super.onListenerConnected()
        sessions = getSystemService(MediaSessionManager::class.java)
        val component = ComponentName(this, MediaBridgeService::class.java)
        sessions.addOnActiveSessionsChangedListener(listener, component)
        updateSessions(sessions.getActiveSessions(component))
    }

    override fun onListenerDisconnected() {
        if (::sessions.isInitialized) {
            sessions.removeOnActiveSessionsChangedListener(listener)
        }
        repository.updateControllers(emptyList())
        super.onListenerDisconnected()
    }

    private fun updateSessions(controllers: List<MediaController>?) =
        repository.updateControllers(controllers.orEmpty())
}
