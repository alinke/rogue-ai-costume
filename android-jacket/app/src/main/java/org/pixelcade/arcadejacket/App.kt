package org.pixelcade.arcadejacket

// Arcade Jacket -- the remote for the LED jacket (LEDLINK, ledlink/): pick a look, ride
// with turn signals and an automatic brake light, light it to the music around you, or
// pulse it with your heart rate. The Android counterpart of the iPhone app (ios-jacket/).
//
// The pieces live here, not in the activity, so they keep running with the screen off
// (JacketService keeps the process in the foreground while the jacket is connected).

import android.app.Application
import androidx.compose.runtime.snapshotFlow
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch

class App : Application() {
    lateinit var link: JacketLink; private set
    lateinit var heart: HeartRate; private set
    lateinit var music: MusicListener; private set
    lateinit var brake: BikeBrake; private set

    private val scope = MainScope()

    override fun onCreate() {
        super.onCreate()
        link = JacketLink(this)
        heart = HeartRate(this)
        music = MusicListener()
        brake = BikeBrake(this)
        // The live sources feed the jacket straight from where they're measured.
        music.onLevels = { b -> link.sendLive(byteArrayOf('M'.code.toByte()) + b) }
        heart.onBpm = { bpm -> link.sendLive(byteArrayOf('H'.code.toByte(), minOf(255, bpm).toByte())) }
        brake.onBrake = { on -> link.send(JacketCommand.BRAKE, if (on) 1 else 0) }

        // The "connected" notification: up from the first connection, with the brake
        // light's and music's needs (location, microphone) as they change.
        scope.launch {
            combine(
                snapshotFlow { link.state == JacketLink.State.CONNECTED },
                snapshotFlow { brake.enabled },
                snapshotFlow { music.running },
            ) { connected, brakeOn, musicOn -> Triple(connected, brakeOn, musicOn) }
                .distinctUntilChanged()
                .collect { (connected, _, _) -> if (connected || JacketService.running) JacketService.update(this@App) }
        }
    }

    /** Bluetooth permissions granted: look for the jacket, and the heart-rate sensor used last time. */
    fun bluetoothReady() {
        link.start()
        heart.start()
    }
}
