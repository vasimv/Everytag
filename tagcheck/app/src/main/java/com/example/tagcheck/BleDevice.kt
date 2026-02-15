package com.example.tagcheck

data class BleDevice(
    val mac: String,
    var rssi: Int,
    val type: String,
    val hex: String
)
