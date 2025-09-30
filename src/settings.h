#ifndef _SETTINGS_H
#define _SETTINGS_H

// Every airtag key requires 28 bytes in RAM and in flash storage
#define MAX_KEYS_IN_MEMORY 40

// 1 - broadcast Google Find My, 0 - disabled
extern int flagFmdn;
// 1 - broadcast Airtag, 0 - disabled
extern int flagAirtag;
// 1, 2, 4, 8 seconds period for broadcasts
extern int multPeriod;
// 0 - low power (-8dbm), 1 - normal power (0dbm), 2 - high power (4dbm or 8dbm)
extern int txPower;
// Interval for keys change (600)
extern int changeInterval;
// Status byte behaviour
// first two bytes - specific byte to broadcast as status for Airtag and FMDN
// 0x10000 - broadcast first byte as status in airtag
// 0x100000 - broadcast second byte as status in FMDN
// 0x20000 - broadcast incrementing (every changeInterval) byte as status in airtag
// 0x200000 - broadcast incrementing byte as status in FMDN
// 0x30000 - broadcast battery voltage as status in airtag (32 - 3.2V)
// 0x300000 - broadcast battery voltage as status in FMDN
// 0x40000 - broadcast battery level (0..3) in airtag with first byte, binary OR
// 0x400000 - broadcast battery level (1..3) in FMDN with second byte, binary OR
// 0x50000 - every minute switch between broadcasting battery voltage, accelerometer and temperature in airtag
// 0x500000 - every minute switch between broadcasting battery voltage, accelerometer and temperature in airtag
extern int statusFlags;
// needs to be reset after settings update
extern int needsReset;
// needs additional pause (minute or two) to upload firmware after authorization
extern int pauseUpload;

// Number of airtag keys loaded
extern int numKeys;
// airtag keys
extern unsigned char (*currentKeys)[28];
// Google Find My Device Network key
extern unsigned char fmdnKey[20];

// Accelerometer threshold to detect movements
// If 0 - then don't use accelerometer even if present on board
extern int accelThreshold;

// MAC address in settings mode
extern uint8_t settingsMAC[6];

// someone connected to the GATT (need some time to receive settings)
extern int connectedGatt;

// Correct authorization key was sent, wait two minutes more if nothing else was sent for possible OTA operation
extern int authorizedGatt;

// Initialize NVS and read settings
int init_settings();

// Start advertisement for settings
void start_settings_adv();

// Stop advertisement for settings, close all connections
void stop_settings_adv();

// Store current time in NVS
void store_time();

// Returns time (in seconds), k_uptime_seconds()+offset
int64_t get_time();

#endif
