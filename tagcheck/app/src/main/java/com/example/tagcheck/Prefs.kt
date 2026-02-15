package com.example.tagcheck

import android.content.Context

object Prefs {

    private const val NAME = "tagcheck_settings"
    private const val KEY_BROADCAST_AIRTAG_UPD = "broadcast_airtag_upd"
    private const val KEY_BROADCAST_FMDN_UPD = "broadcast_fmdn_upd"
    private const val KEY_DEFAULTS_APPLIED_V1 = "defaults_applied_v1"
    private fun p(ctx: Context) =
        ctx.getSharedPreferences(NAME, Context.MODE_PRIVATE)

    /**
     * One-time defaults/migrations. We keep it minimal and safe.
     *
     * In earlier builds some installs ended up with Broadcast FMDN enabled by default.
     * User requested it to be OFF by default.
     */
    fun ensureDefaults(ctx: Context) {
        val sp = p(ctx)
        if (sp.contains(KEY_DEFAULTS_APPLIED_V1)) return
        sp.edit()
            .putBoolean("broadcast_fmdn", false)
            .putBoolean(KEY_DEFAULTS_APPLIED_V1, true)
            .apply()
    }

    /* ========= Target / basic ========= */

    fun getTargetMac(ctx: Context): String =
        p(ctx).getString("target_mac", "") ?: ""

    fun setTargetMac(ctx: Context, v: String) =
        p(ctx).edit().putString("target_mac", v).apply()

    fun getPassword(ctx: Context): String =
        p(ctx).getString("password", "") ?: ""

    fun setPassword(ctx: Context, v: String) =
        p(ctx).edit().putString("password", v).apply()

    /* ========= Config MAC ========= */

    fun getConfigMac(ctx: Context): String =
        p(ctx).getString("config_mac", "") ?: ""

    fun setConfigMac(ctx: Context, v: String) =
        p(ctx).edit().putString("config_mac", v).apply()

    fun getConfigMacUpd(ctx: Context): Boolean =
        p(ctx).getBoolean("config_mac_upd", false)

    fun setConfigMacUpd(ctx: Context, v: Boolean) =
        p(ctx).edit().putBoolean("config_mac_upd", v).apply()

    /* ========= New password ========= */

    fun getNewPassword(ctx: Context): String =
        p(ctx).getString("new_password", "") ?: ""

    fun setNewPassword(ctx: Context, v: String) =
        p(ctx).edit().putString("new_password", v).apply()

    fun getNewPasswordUpd(ctx: Context): Boolean =
        p(ctx).getBoolean("new_password_upd", false)

    fun setNewPasswordUpd(ctx: Context, v: Boolean) =
        p(ctx).edit().putBoolean("new_password_upd", v).apply()

    /* ========= Broadcast flags ========= */

    fun getBroadcastAirtag(ctx: Context): Boolean =
        p(ctx).getBoolean("broadcast_airtag", false)

    fun setBroadcastAirtag(ctx: Context, v: Boolean) =
        p(ctx).edit().putBoolean("broadcast_airtag", v).apply()

    fun getBroadcastFmdn(ctx: Context): Boolean =
        p(ctx).getBoolean("broadcast_fmdn", false)

    fun setBroadcastFmdn(ctx: Context, v: Boolean) =
        p(ctx).edit().putBoolean("broadcast_fmdn", v).apply()

    fun getBroadcastAirtagUpd(ctx: Context): Boolean =
        p(ctx).getBoolean(KEY_BROADCAST_AIRTAG_UPD, false)

    fun getBroadcastFmdnUpd(ctx: Context): Boolean =
        p(ctx).getBoolean(KEY_BROADCAST_FMDN_UPD, false)

    fun setBroadcastAirtagUpd(ctx: Context, value: Boolean) {
        p(ctx).edit().putBoolean(KEY_BROADCAST_AIRTAG_UPD, value).apply()
    }

    fun setBroadcastFmdnUpd(ctx: Context, value: Boolean) {
        p(ctx).edit().putBoolean(KEY_BROADCAST_FMDN_UPD, value).apply()
    }


    /* ========= TX power ========= */

    fun getTxPower(ctx: Context): Int =
        p(ctx).getInt("tx_power", 1) // 0=low,1=normal,2=max

    fun setTxPower(ctx: Context, v: Int) =
        p(ctx).edit().putInt("tx_power", v).apply()

    fun getTxPowerUpd(ctx: Context): Boolean =
        p(ctx).getBoolean("tx_power_upd", false)

    fun setTxPowerUpd(ctx: Context, v: Boolean) =
        p(ctx).edit().putBoolean("tx_power_upd", v).apply()

    /* ========= Intervals ========= */

    fun getKeyInterval(ctx: Context): Int =
        p(ctx).getInt("key_interval", 600)

    fun setKeyInterval(ctx: Context, v: Int) =
        p(ctx).edit().putInt("key_interval", v).apply()

    fun getKeyIntervalUpd(ctx: Context): Boolean =
        p(ctx).getBoolean("key_interval_upd", false)

    fun setKeyIntervalUpd(ctx: Context, v: Boolean) =
        p(ctx).edit().putBoolean("key_interval_upd", v).apply()

    fun getBroadcastPeriod(ctx: Context): Int =
        p(ctx).getInt("broadcast_period", 0)

    fun setBroadcastPeriod(ctx: Context, v: Int) =
        p(ctx).edit().putInt("broadcast_period", v).apply()

    fun getBroadcastPeriodUpd(ctx: Context): Boolean =
        p(ctx).getBoolean("broadcast_period_upd", false)

    fun setBroadcastPeriodUpd(ctx: Context, v: Boolean) =
        p(ctx).edit().putBoolean("broadcast_period_upd", v).apply()

    /* ========= Accel ========= */

    fun getAccelThreshold(ctx: Context): Int =
        p(ctx).getInt("accel_threshold", 800)

    fun setAccelThreshold(ctx: Context, v: Int) =
        p(ctx).edit().putInt("accel_threshold", v).apply()

    fun getAccelThresholdUpd(ctx: Context): Boolean =
        p(ctx).getBoolean("accel_threshold_upd", false)

    fun setAccelThresholdUpd(ctx: Context, v: Boolean) =
        p(ctx).edit().putBoolean("accel_threshold_upd", v).apply()

    /* ========= Status (3 bytes hex) ========= */

    fun getStatus(ctx: Context): String =
        p(ctx).getString("status", "458000") ?: "458000"

    fun setStatus(ctx: Context, v: String) =
        p(ctx).edit().putString("status", v).apply()

    fun getStatusUpd(ctx: Context): Boolean =
        p(ctx).getBoolean("status_upd", false)

    fun setStatusUpd(ctx: Context, v: Boolean) =
        p(ctx).edit().putBoolean("status_upd", v).apply()

    /* ========= FMDN key ========= */

    fun getFmdnKey(ctx: Context): String =
        p(ctx).getString("fmdn_key", "") ?: ""

    fun setFmdnKey(ctx: Context, v: String) =
        p(ctx).edit().putString("fmdn_key", v).apply()

    fun getFmdnKeyUpd(ctx: Context): Boolean =
        p(ctx).getBoolean("fmdn_key_upd", false)

    fun setFmdnKeyUpd(ctx: Context, v: Boolean) =
        p(ctx).edit().putBoolean("fmdn_key_upd", v).apply()

    /* ========= KEYFILE (THIS FIXES YOUR ERROR) ========= */

    fun getKeyfilePath(ctx: Context): String =
        p(ctx).getString("keyfile_path", "") ?: ""

    fun setKeyfilePath(ctx: Context, path: String) =
        p(ctx).edit().putString("keyfile_path", path).apply()

    fun getKeyfileUpd(ctx: Context): Boolean =
        p(ctx).getBoolean("keyfile_upd", false)

    fun setKeyfileUpd(ctx: Context, v: Boolean) =
        p(ctx).edit().putBoolean("keyfile_upd", v).apply()
}
