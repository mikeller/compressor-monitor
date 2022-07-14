package ch.ike.compressormonitor

import android.content.Context
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.flow.flowOn

class CompressorDataSource(
    applicationContext: Context,
    ioDispatcher: CoroutineDispatcher,
    private val refreshIntervalMs: Long = 5000
) {
    val pressureBar: Flow<Double> = flow {
        while(true) {
            val pressureBar = ApiClient.getInstance(applicationContext).getData()
            emit(pressureBar)
            delay(refreshIntervalMs)
        }
    }
    .flowOn(ioDispatcher)
}