package com.example.tagcheck

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Toast
import android.provider.OpenableColumns
import androidx.appcompat.app.AppCompatActivity
import com.example.tagcheck.databinding.ActivitySettingsBinding
import java.io.File

class SettingsActivity : AppCompatActivity() {

    private lateinit var binding: ActivitySettingsBinding


    private var formattingTargetMac = false
    private var formattingConfigMac = false

    private fun isValidAscii8(s: String): Boolean {
        if (s.length != 8) return false
        return s.all { it.code in 0..0x7F }
    }

    private fun updatePasswordErrorViews() {
        val pw = binding.etPassword.text?.toString() ?: ""
        val showPwErr = pw.isNotEmpty() && !isValidAscii8(pw)
        binding.tvPasswordError.visibility = if (showPwErr) android.view.View.VISIBLE else android.view.View.GONE
        binding.etPassword.error = if (showPwErr) "8 ASCII chars" else null

        val npw = binding.etNewPassword.text?.toString() ?: ""
        val showNewPwErr = npw.isNotEmpty() && !isValidAscii8(npw)
        binding.tvNewPasswordError.visibility = if (showNewPwErr) android.view.View.VISIBLE else android.view.View.GONE
        binding.etNewPassword.error = if (showNewPwErr) "8 ASCII chars" else null
    }
    private val pickKeyfile =
        registerForActivityResult(androidx.activity.result.contract.ActivityResultContracts.GetContent()) { uri ->
            if (uri != null) handleKeyfile(uri)
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivitySettingsBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupSpinners()
        setupValidation()

        // Default disabled until we validate loaded / typed values.
        binding.cbKeyfileUpd.isEnabled = false
        binding.cbFmdnKeyUpd.isEnabled = false
        binding.cbConfigMacUpd.isEnabled = false
        binding.cbNewPasswordUpd.isEnabled = false

        loadFromPrefs()

        // Password must be exactly 8 ASCII chars.
        binding.etPassword.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(s: Editable?) {
                updatePasswordErrorViews()
            }
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
        })
        binding.etFmdnKey.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(s: Editable?) {
                val ok = isValidHex(s.toString(), 20)
                binding.cbFmdnKeyUpd.isEnabled = ok
                if (!ok) binding.cbFmdnKeyUpd.isChecked = false
            }

            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
        })


        binding.etTargetMac.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(s: Editable?) {
                if (formattingTargetMac) return
                val formatted = formatMac(s?.toString() ?: "")
                if (formatted != (s?.toString() ?: "")) {
                    formattingTargetMac = true
                    binding.etTargetMac.setText(formatted)
                    binding.etTargetMac.setSelection(formatted.length)
                    formattingTargetMac = false
                }
            }
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
        })

        binding.etConfigMac.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(s: Editable?) {
                if (formattingConfigMac) return
                val formatted = formatMac(s?.toString() ?: "")
                if (formatted != (s?.toString() ?: "")) {
                    formattingConfigMac = true
                    binding.etConfigMac.setText(formatted)
                    binding.etConfigMac.setSelection(formatted.length)
                    formattingConfigMac = false
                    return
                }
                val ok = isValidMac(formatted)
                binding.cbConfigMacUpd.isEnabled = ok
                if (!ok) binding.cbConfigMacUpd.isChecked = false
            }
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
        })

        binding.etNewPassword.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(s: Editable?) {
                val ok = isValidAscii8(s?.toString() ?: "")
                binding.cbNewPasswordUpd.isEnabled = ok
                if (!ok) binding.cbNewPasswordUpd.isChecked = false
                updatePasswordErrorViews()
            }
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
        })

        setupListeners()

        binding.btnBack.setOnClickListener {
            // Return to the main screen.
            finish()
        }

        binding.btnSendToBeacon.setOnClickListener {
            // Save happens in onPause(), then we open Connect screen.
            startActivity(Intent(this, ConnectActivity::class.java))
        }

        // Initial state of error labels
        updatePasswordErrorViews()
    }

    // ----------------------------------------------------
    // Spinners
    // ----------------------------------------------------
    private fun setupSpinners() {
        // TX Power
        val txAdapter = ArrayAdapter(
            this,
            android.R.layout.simple_spinner_dropdown_item,
            listOf("Low", "Normal", "Max")
        )
        binding.spTxPower.adapter = txAdapter

        // Broadcast period (seconds)
        val periodAdapter = ArrayAdapter(
            this,
            android.R.layout.simple_spinner_dropdown_item,
            listOf(1, 2, 4)
        )
        binding.spBroadcastPeriod.adapter = periodAdapter
    }

    // ----------------------------------------------------
    // Load prefs
    // ----------------------------------------------------
    private fun loadFromPrefs() {
        binding.etTargetMac.setText(formatMac(Prefs.getTargetMac(this)))
        binding.etPassword.setText(Prefs.getPassword(this))

        binding.etConfigMac.setText(formatMac(Prefs.getConfigMac(this)))
        binding.cbConfigMacUpd.isChecked = Prefs.getConfigMacUpd(this)

        binding.etNewPassword.setText(Prefs.getNewPassword(this))
        binding.cbNewPasswordUpd.isChecked = Prefs.getNewPasswordUpd(this)

        binding.cbBroadcastAirtag.isChecked = Prefs.getBroadcastAirtag(this)
        binding.cbBroadcastFmdn.isChecked = Prefs.getBroadcastFmdn(this)

        binding.cbBroadcastAirtagUpd.isChecked = Prefs.getBroadcastAirtagUpd(this)
        binding.cbBroadcastFmdnUpd.isChecked = Prefs.getBroadcastFmdnUpd(this)

        binding.spTxPower.setSelection(Prefs.getTxPower(this))
        binding.cbTxPowerUpd.isChecked = Prefs.getTxPowerUpd(this)

        binding.etKeyInterval.setText(
            Prefs.getKeyInterval(this).takeIf { it > 0 }?.toString() ?: "600"
        )
        binding.cbKeyIntervalUpd.isChecked = Prefs.getKeyIntervalUpd(this)

        binding.etAccelThreshold.setText(
            Prefs.getAccelThreshold(this).takeIf { it > 0 }?.toString() ?: "800"
        )
        binding.cbAccelThresholdUpd.isChecked = Prefs.getAccelThresholdUpd(this)

        binding.spBroadcastPeriod.setSelection(
            when (Prefs.getBroadcastPeriod(this)) {
                2 -> 1
                4 -> 2
                else -> 0
            }
        )
        binding.cbBroadcastPeriodUpd.isChecked = Prefs.getBroadcastPeriodUpd(this)

        binding.etStatus.setText(Prefs.getStatus(this))
        binding.cbStatusUpd.isChecked = Prefs.getStatusUpd(this)

        binding.etFmdnKey.setText(Prefs.getFmdnKey(this))
        binding.cbFmdnKeyUpd.isChecked = Prefs.getFmdnKeyUpd(this)

        // Enforce validation-dependent update toggles
        val fmdnOk = isValidHex(binding.etFmdnKey.text.toString(), 20)
        binding.cbFmdnKeyUpd.isEnabled = fmdnOk
        if (!fmdnOk) binding.cbFmdnKeyUpd.isChecked = false

        val cfgOk = isValidMac(binding.etConfigMac.text.toString())
        binding.cbConfigMacUpd.isEnabled = cfgOk
        if (!cfgOk) binding.cbConfigMacUpd.isChecked = false

        val pwOk = isValidAscii8(binding.etNewPassword.text?.toString() ?: "")
        binding.cbNewPasswordUpd.isEnabled = pwOk
        if (!pwOk) binding.cbNewPasswordUpd.isChecked = false

        updatePasswordErrorViews()

        val keyfile = Prefs.getKeyfilePath(this)
        if (keyfile.isNotEmpty()) {
            val f = File(keyfile)
            binding.tvKeyfileStatus.text = f.name
            // Re-enable update checkbox if the saved file is still valid.
            if (f.exists()) {
                val size = f.length()
                val valid = (size - 1) % 28 == 0L && (size - 1) / 28 in 1..39
                binding.cbKeyfileUpd.isEnabled = valid
            }
        }
        binding.cbKeyfileUpd.isChecked = Prefs.getKeyfileUpd(this)
        if (!binding.cbKeyfileUpd.isEnabled) binding.cbKeyfileUpd.isChecked = false
    }

    // ----------------------------------------------------
    // Listeners
    // ----------------------------------------------------
    private fun setupListeners() {
        binding.btnSelectKeyfile.setOnClickListener {
            pickKeyfile.launch("*/*")
        }

        // Buttons are handled in onCreate().

        binding.spTxPower.onItemSelectedListener =
            simpleListener { Prefs.setTxPower(this, it) }

        binding.spBroadcastPeriod.onItemSelectedListener =
            simpleListener {
                val sec = listOf(1, 2, 4)[it]
                Prefs.setBroadcastPeriod(this, sec)
            }
    }

    private fun simpleListener(block: (Int) -> Unit) =
        object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(
                parent: AdapterView<*>, view: android.view.View?, position: Int, id: Long
            ) = block(position)

            override fun onNothingSelected(parent: AdapterView<*>) {}
        }

    // ----------------------------------------------------
    // Validation
    // ----------------------------------------------------
    private fun setupValidation() {
        binding.etStatus.addTextChangedListener(hexWatcher(6))
        binding.etFmdnKey.addTextChangedListener(hexWatcher(null))
    }

    private fun hexWatcher(maxLen: Int?): TextWatcher =
        object : TextWatcher {
            override fun afterTextChanged(s: Editable?) {
                s ?: return
                val clean = s.toString().uppercase().filter { it in "0123456789ABCDEF" }
                if (clean != s.toString()) {
                    s.replace(0, s.length, clean)
                }
                maxLen?.let {
                    if (clean.length > it) s.delete(it, clean.length)
                }
            }
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
        }

    // ----------------------------------------------------
    // Keyfile handling (KEEPING YOUR SIZE CHECKS)
    // ----------------------------------------------------
    private fun handleKeyfile(uri: Uri) {
        val fileName = getFileNameFromUri(uri)
        binding.tvKeyfileStatus.text= fileName

        val file = File(cacheDir, fileName)
        contentResolver.openInputStream(uri)?.use { input ->
            file.outputStream().use { output -> input.copyTo(output) }
        }

        val size = file.length()
        val valid = (size - 1) % 28 == 0L && (size - 1) / 28 in 1..39

        if (!valid) {
            Toast.makeText(
                this,
                "Invalid keyfile size: $size bytes",
                Toast.LENGTH_LONG
            ).show()
            binding.cbKeyfileUpd.isEnabled  = false
            binding.cbKeyfileUpd.isChecked  = false

            return
        }
        binding.cbKeyfileUpd.isEnabled  = true
        Prefs.setKeyfilePath(this, file.absolutePath)
        binding.tvKeyfileStatus.text = file.name
    }

    // ----------------------------------------------------
    // Save on exit
    // ----------------------------------------------------
    override fun onPause() {
        super.onPause()

        Prefs.setTargetMac(this, formatMac(binding.etTargetMac.text.toString()))
        Prefs.setPassword(this, binding.etPassword.text.toString())

        Prefs.setConfigMac(this, formatMac(binding.etConfigMac.text.toString()))
        Prefs.setConfigMacUpd(this, binding.cbConfigMacUpd.isChecked)

        Prefs.setNewPassword(this, binding.etNewPassword.text.toString())
        Prefs.setNewPasswordUpd(this, binding.cbNewPasswordUpd.isChecked)

        Prefs.setBroadcastAirtag(this, binding.cbBroadcastAirtag.isChecked)
        Prefs.setBroadcastFmdn(this, binding.cbBroadcastFmdn.isChecked)

        Prefs.setBroadcastAirtagUpd(this, binding.cbBroadcastAirtagUpd.isChecked)
        Prefs.setBroadcastFmdnUpd(this, binding.cbBroadcastFmdnUpd.isChecked)

        Prefs.setTxPowerUpd(this, binding.cbTxPowerUpd.isChecked)

        Prefs.setKeyInterval(this, binding.etKeyInterval.text.toString().toIntOrNull() ?: 600)
        Prefs.setKeyIntervalUpd(this, binding.cbKeyIntervalUpd.isChecked)

        Prefs.setAccelThreshold(this, binding.etAccelThreshold.text.toString().toIntOrNull() ?: 800)
        Prefs.setAccelThresholdUpd(this, binding.cbAccelThresholdUpd.isChecked)

        Prefs.setBroadcastPeriodUpd(this, binding.cbBroadcastPeriodUpd.isChecked)

        Prefs.setStatus(this, binding.etStatus.text.toString())
        Prefs.setStatusUpd(this, binding.cbStatusUpd.isChecked)

        Prefs.setFmdnKey(this, binding.etFmdnKey.text.toString())
        Prefs.setFmdnKeyUpd(this, binding.cbFmdnKeyUpd.isChecked)

        Prefs.setKeyfileUpd(this, binding.cbKeyfileUpd.isChecked)
    }
    private fun getFileNameFromUri(uri: Uri): String {
        var name = "unknown"
        contentResolver.query(uri, null, null, null, null)?.use { cursor ->
            val idx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (idx >= 0 && cursor.moveToFirst()) {
                name = cursor.getString(idx)
            }
        }
        return name
    }

    private fun formatMac(raw: String): String {
        // Keep only hex chars, uppercase, and auto-insert ':' every 2 hex chars (up to 6 bytes).
        val hex = raw.uppercase().filter { it in "0123456789ABCDEF" }.take(12)
        val out = StringBuilder()
        for (i in hex.indices) {
            out.append(hex[i])
            if (i % 2 == 1 && i != hex.lastIndex) out.append(':')
        }
        return out.toString()
    }

    private fun isValidMac(mac: String): Boolean {
        return mac.matches(Regex("^([0-9A-F]{2}:){5}[0-9A-F]{2}$"))
    }

    private fun isValidHex(s: String, bytes: Int): Boolean {
        if (s.length != bytes * 2) return false
        return s.matches(Regex("^[0-9a-fA-F]+$"))
    }
}
