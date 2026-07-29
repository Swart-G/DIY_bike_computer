package com.diybikecomputer.companion.navigation

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

class NavigationRepository {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private val mutableState = MutableStateFlow(NavigationSnapshot())
    val state: StateFlow<NavigationSnapshot> = mutableState.asStateFlow()
    private var providerJob: Job? = null

    fun attachProvider(provider: NavigationProvider?) {
        providerJob?.cancel()
        providerJob = null
        if (provider == null || !provider.available) {
            mutableState.value = NavigationSnapshot()
            return
        }
        providerJob = scope.launch {
            provider.state.collect { mutableState.value = it.copy(available = true) }
        }
    }
}
