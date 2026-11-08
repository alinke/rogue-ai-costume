package org.pixelcade.arcadejacket.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

val Neon = Color(0xFF00DCFF)
val Panel = Color.White.copy(alpha = 0.06f)
val Dim = Color.White.copy(alpha = 0.6f)
val Orange = Color(0xFFFF9500)

fun rgb(c: Int) = Color(0xFF000000.toInt() or c)

@Composable
fun AppTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = Neon,
            onPrimary = Color.Black,
            background = Color.Black,
            surface = Color(0xFF121212),
            onSurface = Color.White,
            onBackground = Color.White,
        ),
        content = content,
    )
}
