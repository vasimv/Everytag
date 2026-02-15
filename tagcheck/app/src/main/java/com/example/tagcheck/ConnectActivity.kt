package com.example.tagcheck

import android.bluetooth.*
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.os.*
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import com.example.tagcheck.databinding.ActivityConnectBinding
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.io.File
import java.util.*

class ConnectActivity : AppCompatActivity() {

    private lateinit var binding: ActivityConnectBinding
    private var gatt: BluetoothGatt? = null

    private lateinit var scanner: android.bluetooth.le.BluetoothLeScanner
    private var scanning = false
    private var targetMac: String = ""
    private val scanHandler = Handler(Looper.getMainLooper())

    private val handler = Handler(Looper.getMainLooper())
    private val startTime = System.currentTimeMillis()
    private var connected = false
    private var attempt = 0

    private val TIMEOUT_MS = 150_000L
    private val RETRY_DELAY_MS = 1_200L
    private val SCAN_TIMEOUT_MS = 6_000L
    private val CONNECT_DELAY_AFTER_SCAN_MS = 400L

    // UUIDs
    private val passwordUuid =
        UUID.fromString("8c5debdf-ad8d-4810-a31f-53862e79ee77")

    private val txPowerUuid =
        UUID.fromString("8c5debe1-ad8d-4810-a31f-53862e79ee77")

    private val broadcastAirtagUuid =
        UUID.fromString("8c5debdc-ad8d-4810-a31f-53862e79ee77")

    private val broadcastFmdnUuid =
        UUID.fromString("8c5debdb-ad8d-4810-a31f-53862e79ee77")

    private val keyfileUuid =
        UUID.fromString("8c5debde-ad8d-4810-a31f-53862e79ee77")

    private val statusUuid =
        UUID.fromString("8c5debe5-ad8d-4810-a31f-53862e79ee77")

    private val keyIntervalUuid =
        UUID.fromString("8c5debe0-ad8d-4810-a31f-53862e79ee77")

    private val accelThresholdUuid =
        UUID.fromString("8c5debe6-ad8d-4810-a31f-53862e79ee77")

    private val broadcastPeriodUuid =
        UUID.fromString("8c5debdd-ad8d-4810-a31f-53862e79ee77")

    private val fmdnKeyUuid =
        UUID.fromString("8c5debe2-ad8d-4810-a31f-53862e79ee77")

    private val configMacUuid =
        UUID.fromString("8c5debe4-ad8d-4810-a31f-53862e79ee77")

    // Same attribute is used for password auth and setting a new password.
    private val newPasswordUuid =
        UUID.fromString("8c5debdf-ad8d-4810-a31f-53862e79ee77")

    // Write queue
    private val writeQueue: Queue<Pair<UUID, ByteArray>> = LinkedList()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityConnectBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.btnBack.setOnClickListener { finish() }

        if (!Permissions.hasAll(this)) {
            log("Missing BLE permissions")
            showBack()
            return
        }

        targetMac = Prefs.getTargetMac(this)
        if (targetMac.isEmpty()) {
            log("No MAC address configured")
            showBack()
            return
        }

        val manager = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
        scanner = manager.adapter.bluetoothLeScanner

        log("Starting connection attempts to $targetMac")
        connect()
    }

    private fun connect() {
        if (System.currentTimeMillis() - startTime > TIMEOUT_MS) {
            log("Connection timeout reached")
            showBack()
            return
        }

        attempt++
        log("Connect attempt #$attempt")

        // Always teardown previous GATT before a new attempt.
        teardownGatt()

        // Scan first to obtain a fresh BluetoothDevice instance.
        // This avoids many cases of GATT 133 (stale/invalid device address, stack issues).
        startScanForTarget()
    }

    private fun startScanForTarget() {
        if (scanning) return

        scanning = true
        log("Scanning for $targetMac …")

        val filters = listOf(
            ScanFilter.Builder()
                .setDeviceAddress(targetMac)
                .build()
        )

        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        scanner.startScan(filters, settings, scanCallback)

        scanHandler.postDelayed({
            if (scanning) {
                stopScan()
                log("Scan timeout, retrying…")
                handler.postDelayed({ connect() }, RETRY_DELAY_MS)
            }
        }, SCAN_TIMEOUT_MS)
    }

    private fun stopScan() {
        if (!scanning) return
        scanning = false
        try {
            scanner.stopScan(scanCallback)
        } catch (_: Exception) {
        }
        scanHandler.removeCallbacksAndMessages(null)
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device ?: return
            if (!device.address.equals(targetMac, ignoreCase = true)) return

            log("Found target: ${device.address} (RSSI ${result.rssi})")
            stopScan()

            // Small delay after stopping scan helps Android BLE stack stability.
            handler.postDelayed({
                connectToDevice(device)
            }, CONNECT_DELAY_AFTER_SCAN_MS)
        }

        override fun onScanFailed(errorCode: Int) {
            log("Scan failed: $errorCode")
            stopScan()
            handler.postDelayed({ connect() }, RETRY_DELAY_MS)
        }
    }

    private fun connectToDevice(device: BluetoothDevice) {
        log("Connecting GATT to ${device.address}")

        gatt = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            device.connectGatt(this, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        } else {
            device.connectGatt(this, false, gattCallback)
        }
    }

    private fun teardownGatt() {
        try {
            gatt?.disconnect()
        } catch (_: Exception) {
        }
        try {
            gatt?.close()
        } catch (_: Exception) {
        }
        gatt = null
    }

    private val gattCallback = object : BluetoothGattCallback() {

        override fun onConnectionStateChange(
            g: BluetoothGatt,
            status: Int,
            newState: Int
        ) {
            log("State change: status=$status state=$newState")

            if (status != BluetoothGatt.GATT_SUCCESS) {
                retry(g, "GATT error $status")
                return
            }

            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    connected = true
                    log("Connected, discovering services")
                    g.discoverServices()
                }

                BluetoothProfile.STATE_DISCONNECTED -> {
                    if (!connected) {
                        retry(g, "Disconnected before connect")
                    } else {
                        cleanup(g)
                    }
                }
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            log("Services discovered status=$status")

            if (status != BluetoothGatt.GATT_SUCCESS) {
                retry(g, "Service discovery failed")
                return
            }

            writeQueue.clear()

            // 1️⃣ Always write password first
            run {
                val pw = Prefs.getPassword(this@ConnectActivity)
                val pwBytes = pw.toByteArray(Charsets.US_ASCII)
                if (pwBytes.size != 8) {
                    log("Password must be exactly 8 ASCII chars")
                    cleanup(g)
                    return
                }
                writeQueue.add(passwordUuid to pwBytes)
            }

            // 2️⃣ TX Power (Int32)
            if (Prefs.getTxPowerUpd(this@ConnectActivity)) {
                val txPower = Prefs.getTxPower(this@ConnectActivity)

                writeQueue.add(
                    txPowerUuid to intToBytes(txPower)
                )
            }

            // 3️⃣ Broadcast Airtag (Int32 0/1)
            if (Prefs.getBroadcastAirtagUpd(this@ConnectActivity)) {
                val value = if (Prefs.getBroadcastAirtag(this@ConnectActivity)) 1 else 0

                writeQueue.add(
                    broadcastAirtagUuid to intToBytes(value)
                )
            }

            // 4️⃣ Broadcast FMDN (Int32 0/1)
            if (Prefs.getBroadcastFmdnUpd(this@ConnectActivity)) {
                val value = if (Prefs.getBroadcastFmdn(this@ConnectActivity)) 1 else 0

                writeQueue.add(
                    broadcastFmdnUuid to intToBytes(value)
                )
            }

            // 5️⃣ Airtag keyfile (binary, omit first byte, send 14-byte chunks, then 2x 14 zeros)
            if (Prefs.getKeyfileUpd(this@ConnectActivity)) {
                val path = Prefs.getKeyfilePath(this@ConnectActivity)
                if (path.isBlank()) {
                    log("Keyfile update requested, but no file selected")
                } else {
                    try {
                        val raw = File(path).readBytes()
                        if (raw.size < 2) {
                            log("Keyfile too small: ${raw.size} bytes")
                        } else {
                            val payload = raw.copyOfRange(1, raw.size) // omit first byte
                            var off = 0
                            while (off < payload.size) {
                                val chunk = payload.copyOfRange(off, minOf(off + 14, payload.size))
                                // Keyfile is validated in Settings as (size-1)%28==0, so chunks are always 14
                                writeQueue.add(keyfileUuid to chunk)
                                off += 14
                            }

                            // After whole file sent — send two 14-byte zero blocks
                            writeQueue.add(keyfileUuid to ByteArray(14))
                            writeQueue.add(keyfileUuid to ByteArray(14))
                        }
                    } catch (e: Exception) {
                        log("Failed to read keyfile: ${e.message}")
                    }
                }
            }

            // 6️⃣ Status (Int32)
            if (Prefs.getStatusUpd(this@ConnectActivity)) {
                val s = Prefs.getStatus(this@ConnectActivity).trim()
                val statusValue = parseIntFlexible(s)
                writeQueue.add(statusUuid to intToBytes(statusValue))
            }

            // 7️⃣ Key interval (Int32)
            if (Prefs.getKeyIntervalUpd(this@ConnectActivity)) {
                val v = Prefs.getKeyInterval(this@ConnectActivity)
                writeQueue.add(keyIntervalUuid to intToBytes(v))
            }

            // 8️⃣ Accel threshold (Int32)
            if (Prefs.getAccelThresholdUpd(this@ConnectActivity)) {
                val v = Prefs.getAccelThreshold(this@ConnectActivity)
                writeQueue.add(accelThresholdUuid to intToBytes(v))
            }

            // 9️⃣ Broadcast period (Int32)
            if (Prefs.getBroadcastPeriodUpd(this@ConnectActivity)) {
                val v = Prefs.getBroadcastPeriod(this@ConnectActivity)
                writeQueue.add(broadcastPeriodUuid to intToBytes(v))
            }

            // 🔟 FMDN key (binary from hex string)
            if (Prefs.getFmdnKeyUpd(this@ConnectActivity)) {
                val hex = Prefs.getFmdnKey(this@ConnectActivity).trim()
                val bytes = hexToBytes(hex)
                if (bytes == null) {
                    log("FMDN key update requested, but value is not valid hex")
                } else {
                    writeQueue.add(fmdnKeyUuid to bytes)
                }
            }

            // 1️⃣1️⃣ Config MAC (6 bytes, MSB first)
            if (Prefs.getConfigMacUpd(this@ConnectActivity)) {
                val mac = Prefs.getConfigMac(this@ConnectActivity).trim()
                val bytes = macToBytes(mac)
                if (bytes == null || bytes.size != 6) {
                    log("Config MAC update requested, but MAC is not valid")
                } else {
                    writeQueue.add(configMacUuid to bytes)
                }
            }

            // 1️⃣2️⃣ New password (8 chars) — written to the same attribute as password
            if (Prefs.getNewPasswordUpd(this@ConnectActivity)) {
                val pw = Prefs.getNewPassword(this@ConnectActivity)
                val pwBytes = pw.toByteArray(Charsets.US_ASCII)
                if (pwBytes.size != 8) {
                    log("New password update requested, but value is not exactly 8 ASCII chars")
                } else {
                    writeQueue.add(newPasswordUuid to pwBytes)
                }
            }

            writeNextCharacteristic(g)
        }

        override fun onCharacteristicWrite(
            g: BluetoothGatt,
            c: BluetoothGattCharacteristic,
            status: Int
        ) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                log("Write successful: ${c.uuid}")
                writeNextCharacteristic(g)
            } else {
                log("Write failed for ${c.uuid} status=$status")
                cleanup(g)
            }
        }
    }

    private fun writeNextCharacteristic(g: BluetoothGatt) {
        val next = writeQueue.poll()

        if (next == null) {
            log("All writes completed")
            cleanup(g)
            return
        }

        val (uuid, value) = next

        val characteristic = g.services
            .flatMap { it.characteristics }
            .find { it.uuid == uuid }

        if (characteristic == null) {
            log("Characteristic $uuid not found")
            writeNextCharacteristic(g)
            return
        }

        characteristic.value = value

        log("Writing characteristic $uuid")
        g.writeCharacteristic(characteristic)
    }

    private fun intToBytes(value: Int): ByteArray {
        return ByteBuffer
            .allocate(4)
            .order(ByteOrder.LITTLE_ENDIAN) // Change if your device expects BIG_ENDIAN
            .putInt(value)
            .array()
    }

    private fun parseIntFlexible(s: String): Int {
        if (s.isBlank()) return 0
        // Accept decimal ("123") OR hex ("458000" or "0x458000").
        val t = s.removePrefix("0x").removePrefix("0X")
        val isHex = t.all { it in "0123456789abcdefABCDEF" }
        return try {
            if (isHex) t.toLong(16).toInt() else t.toInt()
        } catch (_: Exception) {
            0
        }
    }

    private fun hexToBytes(hex: String): ByteArray? {
        val clean = hex.trim().replace(" ", "")
        if (clean.isEmpty()) return ByteArray(0)
        if (clean.length % 2 != 0) return null
        if (!clean.all { it in "0123456789abcdefABCDEF" }) return null

        return try {
            ByteArray(clean.length / 2) { i ->
                clean.substring(i * 2, i * 2 + 2).toInt(16).toByte()
            }
        } catch (_: Exception) {
            null
        }
    }

    private fun macToBytes(mac: String): ByteArray? {
        // Accept "AA:BB:CC:DD:EE:FF" or "AABBCCDDEEFF".
        val clean = mac.trim().replace(":", "").replace(" ", "").uppercase()
        if (clean.length != 12) return null
        if (!clean.all { it in "0123456789ABCDEF" }) return null

        return try {
            // Device expects reverse order: start from the last byte (LSB first).
            val msbFirst = ByteArray(6) { i ->
                clean.substring(i * 2, i * 2 + 2).toInt(16).toByte()
            }
            msbFirst.reversedArray()
        } catch (_: Exception) {
            null
        }
    }

    private fun retry(g: BluetoothGatt, reason: String) {
        log("Retry needed: $reason")

        g.disconnect()
        g.close()
        this.gatt = null

        stopScan()

        if (System.currentTimeMillis() - startTime > TIMEOUT_MS) {
            log("Timeout reached, giving up")
            showBack()
            return
        }

        handler.postDelayed({
            connect()
        }, RETRY_DELAY_MS)
    }

    private fun cleanup(g: BluetoothGatt) {
        g.disconnect()
        g.close()
        stopScan()
        showBack()
    }

    private fun showBack() {
        runOnUiThread {
            binding.btnBack.visibility = View.VISIBLE
        }
    }

    private fun log(msg: String) {
        runOnUiThread {
            binding.tvLog.append("$msg\n")
            // Keep the latest messages visible.
            binding.svLog.post { binding.svLog.fullScroll(View.FOCUS_DOWN) }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        stopScan()
        teardownGatt()
        handler.removeCallbacksAndMessages(null)
        scanHandler.removeCallbacksAndMessages(null)
    }
}
