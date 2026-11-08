package org.pixelcade.arcadejacket

// The automatic brake light: the phone's GPS speed, and a brake when it drops fast.
// While slowing, "brake on" is re-sent with each reading; the jacket holds it 1.5 s
// after the last, so it goes off by itself once you stop slowing. The same rule as
// the iPhone app's BikeBrake.swift.
//
// GPS speed updates about once a second, so it lights ~1 s after you start braking
// -- fine for traffic behind you, not instant like a wired brake light. Keeps working
// with the screen off while the app's "connected" notification is up (JacketService).

import android.annotation.SuppressLint
import android.content.Context
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Build
import android.os.Looper
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

class BikeBrake(context: Context) {
    var enabled by mutableStateOf(false); private set
    var speed by mutableStateOf<Double?>(null); private set  // m/s, null = no fix
    var braking by mutableStateOf(false); private set

    var onBrake: ((Boolean) -> Unit)? = null

    private val manager = context.getSystemService(Context.LOCATION_SERVICE) as LocationManager
    private val samples = ArrayDeque<Pair<Long, Double>>()  // (ms, m/s)

    private val listener = LocationListener { loc -> update(loc) }

    /** Needs the location permission (MainActivity asks first). */
    @SuppressLint("MissingPermission")
    fun enable(on: Boolean) {
        enabled = on
        if (on) {
            manager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 0L, 0f, listener, Looper.getMainLooper())
        } else {
            manager.removeUpdates(listener)
            samples.clear()
            speed = null
            if (braking) { braking = false; onBrake?.invoke(false) }
        }
    }

    private fun update(loc: Location) {
        val accurate = Build.VERSION.SDK_INT < 26 || !loc.hasSpeedAccuracy() || loc.speedAccuracyMetersPerSecond < 3
        if (loc.hasSpeed() && accurate) samples.addLast(loc.time to loc.speed.toDouble())
        val now = System.currentTimeMillis()
        while (samples.isNotEmpty() && now - samples.first().first > 3000) samples.removeFirst()
        val last = samples.lastOrNull() ?: run { speed = null; return }
        speed = last.second
        // Compare with the reading ~1.5 s back.
        val was = braking
        val ref = samples.firstOrNull { last.first - it.first in 800..2200 }
        braking = if (ref != null) {
            val dt = (last.first - ref.first) / 1000.0
            val slowing = (ref.second - last.second) / dt
            ref.second >= MIN_SPEED && slowing >= DECEL
        } else false
        if (braking) onBrake?.invoke(true) else if (was) onBrake?.invoke(false)
    }

    companion object {
        /** Slowing at least this fast (m/s^2) from at least MIN_SPEED counts as braking. */
        const val DECEL = 1.1
        const val MIN_SPEED = 2.5  // ~9 km/h, ~5.6 mph
    }
}
