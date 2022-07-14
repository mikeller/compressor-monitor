package ch.ike.compressormonitor

import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat

class PressureNotification(private val context: Context) {
    private val channelId = "CompressorMonitor"
    private val notificationId = 1

    init {
         if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val name = "compressor_monitor"
            val descriptionText = "Pressure notifications for Compressor Monitor"
            val importance = NotificationManager.IMPORTANCE_MAX
            val channel = NotificationChannel(channelId, name, importance).apply {
                description = descriptionText
            }

            val notificationManager: NotificationManager =
                context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            notificationManager.createNotificationChannel(channel)
        }
    }

    fun sendNotification (pressureBar: Double) {
        val builder = NotificationCompat.Builder(context, channelId)
            .setSmallIcon(R.drawable.notification_icon)
            .setContentTitle("Compressor Monitor")
            .setContentText("Pressure is $pressureBar bar")
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .setCategory(NotificationCompat.CATEGORY_ALARM)

        with(NotificationManagerCompat.from(context)) {
            // notificationId is a unique int for each notification that you must define
            notify(notificationId, builder.build())
        }
    }

    fun removeNotification () {
        with(NotificationManagerCompat.from(context)) {
            cancel(notificationId)
        }
    }
}
