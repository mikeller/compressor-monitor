package ch.ike.compressormonitor

import android.content.Context
import com.android.volley.Request
import com.android.volley.RequestQueue
import com.android.volley.toolbox.JsonObjectRequest
import com.android.volley.toolbox.RequestFuture
import com.android.volley.toolbox.Volley
import org.json.JSONObject

class ApiClient(context: Context) {
    companion object {
        @Volatile
        private var INSTANCE: ApiClient? = null

        fun getInstance(context: Context) =
            INSTANCE ?: synchronized(this) {
                INSTANCE ?: ApiClient(context).also {
                    INSTANCE = it
                }
            }
    }

    private val url = "http://127.0.0.1:8080/api/getData"

    fun getData (): Double {
        val future: RequestFuture<JSONObject> = RequestFuture.newFuture()
        val jsonObjectRequest = JsonObjectRequest(
            Request.Method.GET, url, null, future, future)

        requestQueue.add(jsonObjectRequest)

        val response = future.get()
        return response.getDouble("pressureBar")
    }

    private val requestQueue: RequestQueue by lazy {
        // applicationContext is key, it keeps you from leaking the
        // Activity or BroadcastReceiver if someone passes one in.
        Volley.newRequestQueue(context)
    }
}