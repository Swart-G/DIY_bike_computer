package com.diybikecomputer.companion.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val BikeDarkColors = darkColorScheme(
    primary = Color(0xFF55D7C4),
    onPrimary = Color(0xFF071012),
    background = Color(0xFF071012),
    onBackground = Color(0xFFF1F5F4),
    surface = Color(0xFF102023),
    onSurface = Color(0xFFF1F5F4),
    surfaceVariant = Color(0xFF172A2D),
    onSurfaceVariant = Color(0xFF98A6A7),
    error = Color(0xFFF36B62),
)

@Composable
fun BikeComputerTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = BikeDarkColors, content = content)
}
