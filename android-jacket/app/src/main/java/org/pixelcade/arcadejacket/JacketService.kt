package org.pixelcade.arcadejacket

// The "connected to your jacket" notification: a foreground service that keeps the app
// running with the screen off -- the jacket stays connected, and the automatic brake
// light (location) and the music looks (microphone) keep working in a pocket. Started
// with the first connection; Stop in the notification ends it.

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat

class JacketService : Service() {
    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            running = false
            ServiceCompat.stopForeground(this, ServiceCompat.STOP_FOREGROUND_REMOVE)
            stopSelf()
            return START_NOT_STICKY
        }
        val app = application as App
        var types = 0
        if (Build.VERSION.SDK_INT >= 29) {
            types = ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE
            if (app.brake.enabled && Permissions.location(this)) types = types or ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION
            if (app.music.running && Permissions.microphone(this)) types = types or ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
        }
        val connected = app.link.state == JacketLink.State.CONNECTED
        try {
            ServiceCompat.startForeground(this, NOTIFICATION_ID, notification(connected), types)
            running = true
        } catch (e: Exception) {
            // Android won't start a foreground service from the background (12+), or
            // the permission for a type went away: carry on without it.
            running = false
            stopSelf()
        }
        return START_NOT_STICKY
    }

    private fun notification(connected: Boolean): Notification {
        val nm = getSystemService(NotificationManager::class.java)
        if (Build.VERSION.SDK_INT >= 26) {
            nm.createNotificationChannel(NotificationChannel(CHANNEL, "Jacket connection", NotificationManager.IMPORTANCE_LOW))
        }
        val open = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE)
        val stop = PendingIntent.getService(this, 1, Intent(this, JacketService::class.java).setAction(ACTION_STOP), PendingIntent.FLAG_IMMUTABLE)
        return NotificationCompat.Builder(this, CHANNEL)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle("Arcade Jacket")
            .setContentText(if (connected) "Connected to your jacket" else "Looking for your jacket")
            .setOngoing(true)
            .setContentIntent(open)
            .addAction(0, "Stop", stop)
            .build()
    }

    companion object {
        private const val CHANNEL = "jacket"
        private const val NOTIFICATION_ID = 1
        private const val ACTION_STOP = "stop"

        @Volatile var running = false; private set

        /** Starts the service, or refreshes its notification and types. */
        fun update(c: Context) {
            try {
                c.startForegroundService(Intent(c, JacketService::class.java))
            } catch (_: Exception) {
                // Not allowed from the background: it's started next time the app is open.
            }
        }
    }
}
