package ch.ike.compressormonitor

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import ch.ike.compressormonitor.databinding.FragmentMainBinding

class MainFragment : Fragment() {

    private var _binding: FragmentMainBinding? = null
    private val viewModel: DataViewModel by activityViewModels()


    // This property is only valid between onCreateView and
    // onDestroyView.
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {

        _binding = FragmentMainBinding.inflate(inflater, container, false)

        viewModel.pressureBar.observe(viewLifecycleOwner, { pressureBar ->
            printPressure(pressureBar)
        })

        return binding.root
    }

    private fun printPressure(pressureBar: Double) {
        binding.textviewFirst.text = getString(R.string.pressure_text, pressureBar)
    }

    private fun printError(error: String?) {
        binding.errorText.text = getString(R.string.errorText, error)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.buttonReload.setOnClickListener {
            //ApiClient.getInstance(activity!!.applicationContext).getData(::updatePressure, ::printError)
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}