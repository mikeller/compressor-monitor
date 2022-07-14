package ch.ike.compressormonitor

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch


class DataViewModel(application: Application) : AndroidViewModel(application) {
    val pressureBar: MutableLiveData<Double> = MutableLiveData()

    init {
        val compressorDataSource = CompressorDataSource(application, Dispatchers.IO)
        viewModelScope.launch {
            // Trigger the flow and consume its elements using collect
            compressorDataSource.pressureBar.collect { pressureBarValue ->
                pressureBar.value = pressureBarValue
            }
        }
    }
}