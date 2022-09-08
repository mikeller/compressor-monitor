package ch.ike.compressormonitor

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.media.RingtoneManager
import android.os.Build
import androidx.core.app.NotificationCompat


class PressureNotification(private val context: Context) {
    private val channelId = "CompressorMonitor"
    private val notificationId = 1
    private val notificationManager: NotificationManager = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
    private val notificationBuilder: NotificationCompat.Builder
    private val notificationIntent: Intent

    init {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val name = "compressor_monitor"
            val descriptionText = "Pressure notifications for Compressor Monitor"
            val importance = NotificationManager.IMPORTANCE_MAX
            val channel = NotificationChannel(channelId, name, importance).apply {
                description = descriptionText
            }
            channel.setSound(RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM), Notification.AUDIO_ATTRIBUTES_DEFAULT)
            channel.enableVibration(true)
            channel.enableLights(true)

            notificationManager.createNotificationChannel(channel)
        }

        notificationBuilder = NotificationCompat.Builder(context, channelId)

        notificationBuilder.setSmallIcon(R.drawable.notification_icon)
            .setContentTitle("Compressor Monitor")
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .setCategory(NotificationCompat.CATEGORY_ALARM)
            .setSound(RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM))
            .setVibrate(longArrayOf(0L, 100L, 1000L))
            .setLights(0, 1000, 1000)

        notificationIntent = Intent(context, MainActivity::class.java)

        notificationIntent.flags = (Intent.FLAG_ACTIVITY_CLEAR_TOP
                or Intent.FLAG_ACTIVITY_SINGLE_TOP)
    }

    fun sendNotification (pressureBar: Double) {
        notificationBuilder.setContentText("Pressure is $pressureBar bar")

        with(notificationManager) {
            val notification = notificationBuilder.build()
            val intent = PendingIntent.getActivity(context,0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
            notification.contentIntent = intent
            notification.flags = notification.flags or Notification.FLAG_AUTO_CANCEL

            // notificationId is a unique int for each notification that you must define
            notify(notificationId, notification)
        }
    }

    fun removeNotification () {
        with(notificationManager) {
            cancel(notificationId)
        }
    }
}
