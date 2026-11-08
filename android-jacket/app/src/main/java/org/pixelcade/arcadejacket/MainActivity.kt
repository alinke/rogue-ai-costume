package org.pixelcade.arcadejacket

// The one activity: the main screen, Settings and "What you need", and the permission
// prompts -- Bluetooth (Nearby devices) and notifications at the start, the microphone
// when a music look is picked, location when the automatic brake light is turned on.

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.material3.Surface
import androidx.compose.ui.Modifier
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlin.math.sin
import androidx.compose.ui.graphics.Color
import org.pixelcade.arcadejacket.ui.AppTheme
import org.pixelcade.arcadejacket.ui.MainActions
import org.pixelcade.arcadejacket.ui.MainScreen
import org.pixelcade.arcadejacket.ui.SettingsScreen
import org.pixelcade.arcadejacket.ui.WhatYouNeed

class MainActivity : ComponentActivity() {
    private val app get() = application as App
    private var afterGrant: (() -> Unit)? = null

    private val ask = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { result ->
        if (result.isNotEmpty() && result.values.all { it }) afterGrant?.invoke()
        afterGrant = null
    }

    private val askStart = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        if (Permissions.bluetooth(this)) app.bluetoothReady() else app.link.start()  // start() shows "allow Bluetooth"
    }

    /** Runs then() once perms are granted (asking first if needed). */
    private fun need(perms: Array<String>, then: () -> Unit) {
        if (Permissions.granted(this, perms)) { then(); return }
        afterGrant = then
        ask.launch(perms)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        // Screenshots: --ez demo true keeps the app off Bluetooth, so it shows demo mode.
        val demo = intent?.getBooleanExtra("demo", false) == true
        if (demo) {
            app.link.start(scan = false)
        } else if (Permissions.bluetooth(this)) {
            app.bluetoothReady()
            if (!Permissions.granted(this, Permissions.NOTIFICATIONS)) askStart.launch(Permissions.NOTIFICATIONS)
        } else {
            askStart.launch(Permissions.BLUETOOTH + Permissions.NOTIFICATIONS)
        }
        // Screenshots: adb shell am start -n org.pixelcade.arcadejacket/.MainActivity --ez demo true
        //   --ei look 2 --ei turn 1 --ez music true --ei bpm 72 --es screen settings|need
        intent?.let { i ->
            if (i.hasExtra("look")) app.link.setLook(i.getIntExtra("look", 0))
            if (i.hasExtra("turn")) app.link.send(JacketCommand.TURN, i.getIntExtra("turn", 0))
            val bpm = i.getIntExtra("bpm", 0)
            val music = i.getBooleanExtra("music", false)
            if (bpm > 0 || music) lifecycleScope.launch {
                var t = 0.0
                while (true) {
                    t += 1.0 / 30
                    if (bpm > 0 && (t * 30).toInt() % 30 == 0) PreviewEngine.heart(bpm)
                    if (music) {
                        val beat = t % 0.5 < 1.0 / 30
                        val b = 0.5 + 0.5 * sin(t * 2.3); val m = 0.5 + 0.5 * sin(t * 3.1 + 1)
                        PreviewEngine.music((60 + 190 * b).toInt(), (50 + 170 * m).toInt(), (80 + 120 * (0.5 + 0.5 * sin(t * 7))).toInt(), 180, if (beat) 1 else 0)
                    }
                    delay(33)
                }
            }
        }
        val startScreen = intent?.getStringExtra("screen") ?: "main"

        setContent {
            AppTheme {
                var screen by rememberSaveable { mutableStateOf(startScreen) }
                // White text by default: plain Text outside Material surfaces would be black.
                Surface(Modifier.fillMaxSize(), color = Color.Black, contentColor = Color.White) {
                Box(Modifier.fillMaxSize().safeDrawingPadding()) {
                    when (screen) {
                        "settings" -> SettingsScreen(app.link) { screen = "main" }
                        "need" -> WhatYouNeed { screen = "main" }
                        else -> MainScreen(app, MainActions(
                            openSettings = { screen = "settings" },
                            openWhatYouNeed = { screen = "need" },
                            needMicrophone = { then -> need(Permissions.MICROPHONE) { then(); JacketService.update(this@MainActivity) } },
                            needLocation = { then -> need(Permissions.LOCATION) { then(); JacketService.update(this@MainActivity) } },
                            needBluetooth = { askStart.launch(Permissions.BLUETOOTH) },
                        ))
                    }
                }
                }
            }
        }
    }
}
