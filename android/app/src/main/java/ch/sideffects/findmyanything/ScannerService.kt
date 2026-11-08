package ch.sideffects.findmyanything

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
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat

/**
 * Foreground service while the watchapp is open: keeps the process alive and lets
 * Android scan at full speed with the phone in the pocket.
 */
class ScannerService : Service() {
    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        running = false
        super.onDestroy()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            ServiceCompat.stopForeground(this, ServiceCompat.STOP_FOREGROUND_REMOVE)
            stopSelf()
            return START_NOT_STICKY
        }
        try {
            ServiceCompat.startForeground(
                this, NOTIFICATION_ID, notification(this),
                if (Build.VERSION.SDK_INT >= 29) ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE else 0,
            )
        } catch (e: Exception) {
            Log.w("FindMyAnything", "startForeground failed", e)
            stopSelf()
        }
        return START_NOT_STICKY
    }

    companion object {
        private const val CHANNEL = "scan"
        private const val NOTIFICATION_ID = 1
        private const val ACTION_STOP = "ch.sideffects.findmyanything.STOP"
        @Volatile private var running = false

        private fun notification(context: Context): Notification {
            val nm = context.getSystemService(NotificationManager::class.java)
            if (nm != null && nm.getNotificationChannel(CHANNEL) == null) {
                nm.createNotificationChannel(
                    NotificationChannel(CHANNEL, "Suche läuft", NotificationManager.IMPORTANCE_LOW)
                )
            }
            val open = PendingIntent.getActivity(
                context, 0, Intent(context, MainActivity::class.java),
                PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
            )
            return NotificationCompat.Builder(context, CHANNEL)
                .setSmallIcon(R.drawable.ic_stat_radar)
                .setContentTitle("Find My Anything sucht")
                .setContentText("Die Uhr zeigt Bluetooth-Geräte in der Nähe")
                .setOngoing(true)
                .setContentIntent(open)
                .setForegroundServiceBehavior(NotificationCompat.FOREGROUND_SERVICE_IMMEDIATE)
                .build()
        }

        fun start(context: Context) {
            if (running) return
            try {
                ContextCompat.startForegroundService(context, Intent(context, ScannerService::class.java))
                running = true
            } catch (e: Exception) {
                // Android 12+ refuses this from the background unless battery optimisation is off
                Log.w("FindMyAnything", "foreground service not allowed", e)
            }
        }

        fun stop(context: Context) {
            if (!running) return
            running = false
            try {
                context.startService(Intent(context, ScannerService::class.java).setAction(ACTION_STOP))
            } catch (_: Exception) {
                context.stopService(Intent(context, ScannerService::class.java))
            }
        }
    }
}
