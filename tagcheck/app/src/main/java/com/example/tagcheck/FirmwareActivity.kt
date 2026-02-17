package com.example.tagcheck

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.Editable
import android.text.TextWatcher
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.example.tagcheck.databinding.ActivityFirmwareBinding
import io.runtime.mcumgr.ble.McuMgrBleTransport
import io.runtime.mcumgr.dfu.FirmwareUpgradeCallback
import io.runtime.mcumgr.dfu.FirmwareUpgradeController
import io.runtime.mcumgr.dfu.mcuboot.FirmwareUpgradeManager
import io.runtime.mcumgr.dfu.mcuboot.model.ImageSet
import io.runtime.mcumgr.dfu.mcuboot.model.TargetImage
import io.runtime.mcumgr.exception.McuMgrException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.ByteArrayOutputStream
import java.util.Locale
import java.util.UUID
import android.view.View
import android.widget.ScrollView

@SuppressLint("MissingPermission")
class FirmwareActivity : AppCompatActivity() {

    private lateinit var binding: ActivityFirmwareBinding
    private var firmwareUri: Uri? = null

    // ----- Connect logic (same as ConnectActivity) -----
    private var targetMac: String = ""
    private var scanning = false
    private var connected = false
    private var attempt = 0
    private var startTimeMs = 0L

    private val scanHandler = Handler(Looper.getMainLooper())
    private val retryHandler = Handler(Looper.getMainLooper())

    private var scanner: BluetoothLeScanner? = null
    private var unlockGatt: BluetoothGatt? = null

    private val timeoutMs = 150_000L
    private val scanTimeoutMs = 6_000L
    private val retryDelayMs = 1_200L
    private val connectDelayAfterScanMs = 400L

    // Password characteristic UUID (requested)
    private val passwordUuid = UUID.fromString("8c5debdf-ad8d-4810-a31f-53862e79ee77")

    // ----- mcumgr DFU -----
    private var dfuTransport: McuMgrBleTransport? = null
    private var dfuManager: FirmwareUpgradeManager? = null
    private var dfuController: Any? = null // type differs between versions, keep it safe

    // DFU liveness (some devices / stacks hang with no callbacks)
    @Volatile private var dfuAnyCallbackSeen = false
    private val dfuWatchdogHandler = Handler(Looper.getMainLooper())
    // Give enough time for: connect -> discover services -> first SMP response.
    // If nothing happens within this window, we restart the whole flow.
    private val dfuNoCallbackTimeoutMs = 25_000L

    private val dfuCallback = object : FirmwareUpgradeCallback<FirmwareUpgradeManager.State> {

        override fun onUpgradeStarted(controller: FirmwareUpgradeController) {
            dfuController = controller
            dfuAnyCallbackSeen = true
            log("DFU: upgrade started")
        }

        override fun onStateChanged(
            prevState: FirmwareUpgradeManager.State,
            newState: FirmwareUpgradeManager.State
        ) {
            dfuAnyCallbackSeen = true
            log("DFU: $prevState -> $newState")
        }

        override fun onUploadProgressChanged(bytesSent: Int, imageSize: Int, timestamp: Long) {
            dfuAnyCallbackSeen = true
            val pct = if (imageSize > 0) (bytesSent * 100 / imageSize) else 0
            log("DFU: upload $bytesSent / $imageSize ($pct%)")
        }

        override fun onUpgradeCompleted() {
            dfuAnyCallbackSeen = true
            log("DFU: completed")
            cleanupAll()
            resetUi()
        }

        override fun onUpgradeCanceled(state: FirmwareUpgradeManager.State) {
            dfuAnyCallbackSeen = true
            log("DFU: canceled at $state")
            cleanupAll()
            resetUi()
        }

        override fun onUpgradeFailed(state: FirmwareUpgradeManager.State, error: McuMgrException) {
            dfuAnyCallbackSeen = true
            log("DFU: FAILED at $state : ${error.message ?: error}")
            cleanupAll()
            resetUi()
        }
    }

    private val selectFileLauncher =
        registerForActivityResult(ActivityResultContracts.GetContent()) { uri: Uri? ->
            uri?.let {
                firmwareUri = it
                binding.tvFirmwareFileStatus.text = "File: ${it.path}"
                log("Firmware file selected: $it")
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityFirmwareBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.etTargetMac.setText(Prefs.getTargetMac(this))
        binding.etPassword.setText(Prefs.getPassword(this))

        // simple MAC auto-format: AA:BB:...
        binding.etTargetMac.addTextChangedListener(object : TextWatcher {
            private var formatting = false
            override fun afterTextChanged(s: Editable?) {
                if (formatting) return
                val formatted = formatMac(s?.toString() ?: "")
                if (formatted != (s?.toString() ?: "")) {
                    formatting = true
                    binding.etTargetMac.setText(formatted)
                    binding.etTargetMac.setSelection(formatted.length)
                    formatting = false
                }
            }
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
        })

        binding.btnBack.setOnClickListener { finish() }
        binding.btnSelectFirmware.setOnClickListener { selectFileLauncher.launch("*/*") }
        binding.btnFlashFirmware.setOnClickListener { startFlashProcess() }
    }

    override fun onDestroy() {
        super.onDestroy()
        cleanupAll()
        stopScan()
        teardownGatt()
        retryHandler.removeCallbacksAndMessages(null)
        scanHandler.removeCallbacksAndMessages(null)
        dfuWatchdogHandler.removeCallbacksAndMessages(null)
    }

    private fun startFlashProcess() {
        if (!Permissions.hasAll(this)) {
            toast("BLE permissions not granted.")
            Permissions.request(this, 1001)
            return
        }

        val mac = binding.etTargetMac.text.toString()
        val password = binding.etPassword.text.toString()

        if (!BluetoothAdapter.checkBluetoothAddress(mac)) {
            toast("Invalid MAC address.")
            return
        }
        if (!isAsciiPassword8(password)) {
            toast("Password must be exactly 8 ASCII characters.")
            return
        }
        if (firmwareUri == null) {
            toast("Please select a firmware file.")
            return
        }

        Prefs.setTargetMac(this, mac)
        Prefs.setPassword(this, password)

        targetMac = mac
        attempt = 0
        connected = false
        startTimeMs = System.currentTimeMillis()

        binding.btnFlashFirmware.isEnabled = false
        binding.btnBack.isEnabled = false
        binding.btnSelectFirmware.isEnabled = false

        log("Connecting to $targetMac (unlock step)...")

        connect()
    }

    private fun connect() {
        val elapsed = System.currentTimeMillis() - startTimeMs
        if (elapsed >= timeoutMs) {
            log("Timeout reached, giving up.")
            resetUi()
            return
        }

        attempt += 1
        log("Attempt #$attempt")

        teardownGatt()
        stopScan()

        startScanForTarget()
    }

    private fun startScanForTarget() {
        val adapter = getBluetoothAdapter() ?: run {
            log("Bluetooth adapter not available")
            resetUi()
            return
        }
        scanner = adapter.bluetoothLeScanner
        if (scanner == null) {
            log("BLE scanner not available")
            resetUi()
            return
        }

        val filter = ScanFilter.Builder()
            .setDeviceAddress(targetMac)
            .build()

        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        scanning = true
        scanner?.startScan(listOf(filter), settings, scanCallback)

        scanHandler.postDelayed({
            if (scanning) {
                log("Scan timeout, retry...")
                stopScan()
                scheduleRetry()
            }
        }, scanTimeoutMs)
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            if (!scanning) return
            if (result.device.address != targetMac) return

            log("Found target, connecting...")
            stopScan()

            val device = result.device
            retryHandler.postDelayed({
                connectGatt(device)
            }, connectDelayAfterScanMs)
        }

        override fun onScanFailed(errorCode: Int) {
            log("Scan failed: $errorCode, retry...")
            stopScan()
            scheduleRetry()
        }
    }

    private fun connectGatt(device: BluetoothDevice) {
        unlockGatt = if (android.os.Build.VERSION.SDK_INT >= 23) {
            device.connectGatt(this, false, unlockGattCallback, BluetoothDevice.TRANSPORT_LE)
        } else {
            device.connectGatt(this, false, unlockGattCallback)
        }
    }

    private val unlockGattCallback = object : BluetoothGattCallback() {

        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                log("GATT error status=$status, retry...")
                connected = false
                teardownGatt()
                scheduleRetry()
                return
            }

            if (newState == BluetoothProfile.STATE_CONNECTED) {
                connected = true
                log("Connected. Discovering services...")
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                connected = false
                log("Disconnected, retry...")
                teardownGatt()
                scheduleRetry()
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                log("Service discovery failed: $status, retry...")
                teardownGatt()
                scheduleRetry()
                return
            }

            val characteristic = findCharacteristic(gatt, passwordUuid)
            if (characteristic == null) {
                log("Password characteristic not found, retry...")
                teardownGatt()
                scheduleRetry()
                return
            }

            val pass = binding.etPassword.text.toString()
            if (!isAsciiPassword8(pass)) {
                log("Password invalid, stopping.")
                teardownGatt()
                resetUi()
                return
            }

            characteristic.value = pass.toByteArray(Charsets.US_ASCII)
            characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT

            log("Writing password...")
            val ok = gatt.writeCharacteristic(characteristic)
            if (!ok) {
                log("writeCharacteristic returned false, retry...")
                teardownGatt()
                scheduleRetry()
            }
        }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            if (characteristic.uuid != passwordUuid) return

            if (status != BluetoothGatt.GATT_SUCCESS) {
                log("Password write failed: $status, retry...")
                teardownGatt()
                scheduleRetry()
                return
            }

            log("Password OK. Starting firmware upgrade (CONFIRM_ONLY)...")
            teardownGatt()

            val device = getBluetoothAdapter()?.getRemoteDevice(targetMac)
            if (device == null) {
                log("Can't get device after unlock")
                resetUi()
                return
            }

            log("Password OK. Waiting device advertisement before DFU...")
            // Device may reboot / restart advertising after password unlock.
            // Give it a bit more time to become connectable.
            retryHandler.postDelayed({
                startScanForDfuThenStart()
            }, 2500L)

        }
    }
    private fun startScanForDfuThenStart() {
        stopScan()

        val adapter = getBluetoothAdapter() ?: run {
            log("Bluetooth adapter not available")
            resetUi()
            return
        }
        val sc = adapter.bluetoothLeScanner ?: run {
            log("BLE scanner not available")
            resetUi()
            return
        }

        val filter = ScanFilter.Builder().setDeviceAddress(targetMac).build()
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()

        scanning = true
        scanner = sc

        log("DFU scan: waiting for $targetMac ...")
        sc.startScan(listOf(filter), settings, dfuScanCallback)

        // небольшой таймаут скана (как в ConnectActivity)
        scanHandler.postDelayed({
            if (scanning) {
                log("DFU scan timeout, retry connect flow...")
                stopScan()
                scheduleRetry() // вернёмся к connect() и снова unlock+dfu
            }
        }, 6000L)
    }

    private val dfuScanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            if (!scanning) return
            if (result.device.address != targetMac) return

            log("DFU scan: device is advertising, starting DFU...")
            stopScan()

            // Start DFU not immediately: let the BLE stack fully finish the scan stop / previous disconnect.
            retryHandler.postDelayed({
                startDfu(result.device)
            }, 1500L)
        }

        override fun onScanFailed(errorCode: Int) {
            log("DFU scan failed: $errorCode, retry...")
            stopScan()
            scheduleRetry()
        }
    }


    private fun scheduleRetry() {
        val elapsed = System.currentTimeMillis() - startTimeMs
        val left = timeoutMs - elapsed
        if (left <= 0) {
            log("Timeout reached, giving up.")
            resetUi()
            return
        }
        retryHandler.postDelayed({ connect() }, retryDelayMs)
    }

    private fun stopScan() {
        if (!scanning) return
        try {
            scanner?.stopScan(scanCallback)
        } catch (_: Exception) {
        }
        scanning = false
        scanHandler.removeCallbacksAndMessages(null)
        try { scanner?.stopScan(scanCallback) } catch (_: Exception) {}
        try { scanner?.stopScan(dfuScanCallback) } catch (_: Exception) {}
    }

    private fun teardownGatt() {
        val g = unlockGatt
        unlockGatt = null
        if (g != null) {
            try { g.disconnect() } catch (_: Exception) {}
            try { g.close() } catch (_: Exception) {}
        }
    }

    private fun getBluetoothAdapter(): BluetoothAdapter? {
        val bm = getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
        return bm?.adapter
    }

    private fun findCharacteristic(gatt: BluetoothGatt, uuid: UUID): BluetoothGattCharacteristic? {
        try {
            gatt.services?.forEach { svc ->
                svc.characteristics?.firstOrNull { it.uuid == uuid }?.let { return it }
            }
        } catch (_: Exception) {}
        return null
    }

    // ----- DFU -----

    private fun startDfu(device: BluetoothDevice) {
        val uri = firmwareUri ?: run {
            log("DFU: no firmware selected")
            resetUi()
            return
        }

        lifecycleScope.launch(Dispatchers.IO) {
            val fwBytes = try {
                readAllBytesFromUri(uri)
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    log("DFU: can't read firmware: ${e.message}")
                    resetUi()
                }
                return@launch
            }

            // Build manager/transport on Main (safer for some BLE stacks),
            // but START DFU in IO thread to avoid main-thread hangs.
            val settings = FirmwareUpgradeManager.Settings.Builder().build()

            withContext(Dispatchers.Main) {
                try {
                    dfuAnyCallbackSeen = false
                    dfuWatchdogHandler.removeCallbacksAndMessages(null)

                    val transport = McuMgrBleTransport(this@FirmwareActivity, device)
                    trySetMcuMgrTransportTimeouts(transport)
                    dfuTransport = transport

                    val manager = FirmwareUpgradeManager(transport, dfuCallback)
                    dfuManager = manager

                    // Confirm-only (no test) as requested.
                    try { dfuManager?.setMode(FirmwareUpgradeManager.Mode.CONFIRM_ONLY) } catch (_: Throwable) {}

                    // Some builds expose this helper.
                    try {
                        val m = dfuManager?.javaClass?.methods?.firstOrNull {
                            it.name == "setCallbackOnUiThread" && it.parameterTypes.size == 1
                        }
                        m?.invoke(dfuManager, true)
                    } catch (_: Throwable) {}

                    log("DFU: start upload (${fwBytes.size} bytes)...")

                    // Watchdog: if nothing happens, restart whole flow (unlock+dfu)
                    dfuWatchdogHandler.postDelayed({
                        if (!dfuAnyCallbackSeen) {
                            log("DFU: no callbacks for ${dfuNoCallbackTimeoutMs}ms. Retrying...")
                            cleanupAll()
                            scheduleRetry()
                        }
                    }, dfuNoCallbackTimeoutMs)
                } catch (e: Exception) {
                    log("DFU: init failed: ${e.message}")
                    cleanupAll()
                    resetUi()
                    return@withContext
                }
            }

            // Start DFU on background thread.
            try {
                dfuController = tryStartDfu(dfuManager, fwBytes, settings)
                if (dfuController == null) {
                    withContext(Dispatchers.Main) {
                        log("DFU: start() returned null. Retrying...")
                        cleanupAll()
                        scheduleRetry()
                    }
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    log("DFU: start failed: ${e.message}")
                    cleanupAll()
                    scheduleRetry()
                }
            }
        }
    }

    /**
     * Nordic mcumgr transport has a few timeout setters depending on the exact artifact.
     * Use reflection so the code stays compatible with the current dependency setup.
     */
    private fun trySetMcuMgrTransportTimeouts(transport: McuMgrBleTransport) {
        fun tryIntMethod(name: String, value: Int) {
            try {
                val m = transport.javaClass.methods.firstOrNull {
                    it.name == name &&
                            it.parameterTypes.size == 1 &&
                            (it.parameterTypes[0] == Int::class.javaPrimitiveType || it.parameterTypes[0] == Int::class.java)
                }
                m?.invoke(transport, value)
            } catch (_: Throwable) {
            }
        }

        // These names vary between forks/versions; harmless if missing.
        tryIntMethod("setConnectionTimeout", 15_000)
        tryIntMethod("setOperationTimeout", 30_000)
        tryIntMethod("setResponseTimeout", 30_000)
        tryIntMethod("setMtu", 247) // if supported, helps throughput
    }

    private fun tryStartDfu(
        manager: FirmwareUpgradeManager?,
        fwBytes: ByteArray,
        settings: FirmwareUpgradeManager.Settings
    ): Any? {
        if (manager == null) return null

        // Fallback: single-image API (no slot selection).
        return try {
            manager.start(fwBytes, settings)
        } catch (t: Throwable) {
            log("DFU: start(byte[], eraseStorage) failed: ${t.message}")
            null
        }
    }

    private fun readAllBytesFromUri(uri: Uri): ByteArray {
        contentResolver.openInputStream(uri).use { input ->
            requireNotNull(input) { "Can't open file input stream" }
            val out = ByteArrayOutputStream()
            val buf = ByteArray(16 * 1024)
            while (true) {
                val n = input.read(buf)
                if (n <= 0) break
                out.write(buf, 0, n)
            }
            return out.toByteArray()
        }
    }

    private fun cleanupAll() {
        dfuWatchdogHandler.removeCallbacksAndMessages(null)
        try {
            val c = dfuController
            if (c != null) {
                c.javaClass.methods.firstOrNull { it.name == "cancel" && it.parameterTypes.isEmpty() }?.invoke(c)
            }
        } catch (_: Throwable) {}
        dfuController = null

        try {
            val m = dfuManager
            if (m != null) {
                m.javaClass.methods.firstOrNull { it.name == "release" && it.parameterTypes.isEmpty() }?.invoke(m)
                m.javaClass.methods.firstOrNull { it.name == "close" && it.parameterTypes.isEmpty() }?.invoke(m)
            }
        } catch (_: Throwable) {}
        dfuManager = null

        try {
            val t = dfuTransport
            if (t != null) {
                // Prefer real close() if exists.
                try { t.close() } catch (_: Throwable) {}
                t.javaClass.methods.firstOrNull { it.name == "release" && it.parameterTypes.isEmpty() }?.invoke(t)
                t.javaClass.methods.firstOrNull { it.name == "close" && it.parameterTypes.isEmpty() }?.invoke(t)
            }
        } catch (_: Throwable) {}
        dfuTransport = null

        teardownGatt()
        stopScan()
    }

    private fun resetUi() {
        runOnUiThread {
            binding.btnFlashFirmware.isEnabled = true
            binding.btnBack.isEnabled = true
            binding.btnSelectFirmware.isEnabled = true
        }
    }

    private fun log(message: String) {
        runOnUiThread {
            binding.tvLog.append("$message\n")

            // auto-scroll если tvLog лежит внутри ScrollView
            val parent = binding.tvLog.parent
            if (parent is ScrollView) {
                parent.post { parent.fullScroll(View.FOCUS_DOWN) }
            } else {
                // fallback на случай, если обёртка другая
                binding.tvLog.post {
                    binding.tvLog.layout?.let { layout ->
                        val scrollAmount = layout.getLineTop(binding.tvLog.lineCount) - binding.tvLog.height
                        if (scrollAmount > 0) binding.tvLog.scrollTo(0, scrollAmount) else binding.tvLog.scrollTo(0, 0)
                    }
                }
            }
        }
    }
    private fun toast(message: String) {
        runOnUiThread { Toast.makeText(this, message, Toast.LENGTH_SHORT).show() }
    }

    private fun isAsciiPassword8(s: String): Boolean {
        if (s.length != 8) return false
        return s.all { it.code in 0x20..0x7E }
    }

    private fun formatMac(input: String): String {
        val raw = input.uppercase(Locale.US)
            .replace(":", "")
            .filter { it in "0123456789ABCDEF" }
        val sb = StringBuilder()
        for (i in raw.indices) {
            sb.append(raw[i])
            if (i % 2 == 1 && i < raw.length - 1 && sb.length < 17) sb.append(':')
        }
        return sb.toString().take(17)
    }
}

