package com.example.tagcheck

import android.bluetooth.BluetoothManager
import android.os.*
import android.view.View
import android.widget.Button
import androidx.appcompat.app.AppCompatActivity
import com.example.tagcheck.databinding.ActivityScanBinding

class ScanActivity : AppCompatActivity() {

    private lateinit var binding: ActivityScanBinding
    // Keep insertion order stable while scanning.
    private val devices = LinkedHashMap<String, BleDevice>()
    private val deviceButtons = HashMap<String, Button>()
    private lateinit var scanner: android.bluetooth.le.BluetoothLeScanner
    private lateinit var callback: BleScanner
    private val handler = Handler(Looper.getMainLooper())
    private var scanning = false
    private var minRssi = -50

    private val SCAN_TIMEOUT_MS = 30_000L

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityScanBinding.inflate(layoutInflater)
        setContentView(binding.root)

        val manager = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
        scanner = manager.adapter.bluetoothLeScanner

        callback = BleScanner {
            // Filter by RSSI threshold (e.g. -50 means show -50, -40, ... but hide -60, -80, ...)
            if (it.rssi < minRssi) return@BleScanner
            val existed = devices.containsKey(it.mac)
            devices[it.mac] = it
            if (existed) updateItem(it) else addItem(it)
        }

        binding.btnStart.setOnClickListener { startScan() }
        binding.btnStop.setOnClickListener { stopScan() }
        binding.btnBack.setOnClickListener { finish() }
        binding.etMinRssi.setText("50")

        updateStatus()
    }

    private fun startScan() {
        if (scanning) return

        // Validate min RSSI only when START SCAN is pressed.
        val absVal = binding.etMinRssi.text?.toString()?.trim()?.toIntOrNull()
        if (absVal == null || absVal !in 10..90) {
            binding.etMinRssi.error = "Enter 10..90"
            return
        }
        binding.etMinRssi.error = null
        minRssi = -absVal

        devices.clear()
        deviceButtons.clear()
        binding.list.removeAllViews()
        scanning = true
        updateStatus()

        scanner.startScan(callback)

        handler.postDelayed({ stopScan() }, SCAN_TIMEOUT_MS)
    }


    private fun stopScan() {
        if (!scanning) return

        scanning = false
        scanner.stopScan(callback)
        updateStatus()
        binding.btnBack.visibility = View.VISIBLE
    }

    private fun updateStatus() {
        binding.tvStatus.text =
            if (scanning) "Scanning…" else "Not scanning"
    }

    private fun addItem(d: BleDevice) {
        val b = Button(this)
        b.text = formatDeviceText(d)
        b.setOnClickListener {
            Prefs.setTargetMac(this, d.mac)
            finish()
        }
        deviceButtons[d.mac] = b
        binding.list.addView(b)
    }

    private fun updateItem(d: BleDevice) {
        val b = deviceButtons[d.mac] ?: return
        b.text = formatDeviceText(d)
    }

    private fun formatDeviceText(d: BleDevice): String {
        return "${d.mac}\nRSSI ${d.rssi}\n${d.type}\n${d.hex}"
    }

    override fun onDestroy() {
        super.onDestroy()
        handler.removeCallbacksAndMessages(null)
        if (scanning) scanner.stopScan(callback)
    }
}