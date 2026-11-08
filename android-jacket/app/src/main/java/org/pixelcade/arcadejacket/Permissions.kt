package org.pixelcade.arcadejacket

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.content.ContextCompat

/** The runtime permissions each feature needs (MainActivity asks for them). */
object Permissions {
    /** Finding and connecting to the jacket and a heart-rate sensor. Android 11 and older scan only with location on. */
    val BLUETOOTH: Array<String> =
        if (Build.VERSION.SDK_INT >= 31) arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)

    /** The "connected to your jacket" notification that keeps it running with the screen off. */
    val NOTIFICATIONS: Array<String> =
        if (Build.VERSION.SDK_INT >= 33) arrayOf(Manifest.permission.POST_NOTIFICATIONS) else emptyArray()

    val MICROPHONE = arrayOf(Manifest.permission.RECORD_AUDIO)
    val LOCATION = arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)

    fun granted(c: Context, perms: Array<String>) =
        perms.all { ContextCompat.checkSelfPermission(c, it) == PackageManager.PERMISSION_GRANTED }

    fun bluetooth(c: Context) = granted(c, BLUETOOTH)
    fun microphone(c: Context) = granted(c, MICROPHONE)
    fun location(c: Context) = granted(c, LOCATION)
}
