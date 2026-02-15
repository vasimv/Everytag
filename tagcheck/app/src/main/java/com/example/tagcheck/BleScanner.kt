package com.example.tagcheck

import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.os.Build

class BleScanner(
    private val onFound: (BleDevice) -> Unit
) : ScanCallback() {

    override fun onScanResult(type: Int, result: ScanResult) {
        val record = result.scanRecord?.bytes
        val hex = record?.joinToString("") { "%02X".format(it) } ?: ""

        // Android exposes connectable bit reliably on API 26+.
        // On older Android versions this info isn't available, so we don't filter.
        val isConnectable = if (Build.VERSION.SDK_INT >= 26) {
            result.isConnectable
        } else {
            true
        }

        val deviceType = when {
            hex.contains("4C000215") -> "iBeacon"
            hex.contains("4C001219") -> "AirTag"
            isConnectable -> "BLE"
            else -> return
        }

        onFound(
            BleDevice(
                mac = result.device.address,
                rssi = result.rssi,
                type = deviceType,
                hex = hex
            )
        )
    }
}
