// Defines for different boards
#ifndef _MYBOARDS_H
#define _MYBOARDS_H

// Settings mode interval (seconds)
#define INTERVAL_SETTINGS 60

// How long we wait for connection in settings mode (seconds)
#define SETTINGS_WAIT 2

// Settings advertisement interval (in milliseconds)
#define SETTINGS_ADV_INTERVAL 400

// How often we save current time in NVS (seconds)
#define INTERVAL_TIMESAVE 600

// How often we check battery (seconds)
#define INTERVAL_BATTERYCHECK 60

// How often we broadcast with full power (seconds)
#define INTERVAL_MAXPOWER 68

// How many samples to measure battery voltage (including oversampling from .dts)
#define BATTERY_ADC_SAMPLES 3

#if defined(CONFIG_BOARD_WB_20241007)
// Boards with directly connected li-ion battery needs shutdown when voltage is too low
#define BATTERY_MULTIPLIER 5.0
#define BATTERY_UVLO_SHUTDOWN
#define BATTERY_LIION
#else
#define BATTERY_MULTIPLIER 1.0
#endif

#ifdef CONFIG_BOARD_KKM_P1_NRF52810
// KKM P1 has accelerometer and environment sensor (not implemented)
// We have to initialize I2C to preven KXTJ3 from drawing current
#define HAS_I2C_KXTJ3
// Initialize environment sensor for same reason
#define HAS_LED1_SWITCH
#endif

#ifdef CONFIG_BOARD_KKM_K4P
#define HAS_I2C_LIS2DW12
#endif

#ifdef CONFIG_BOARD_WB_20241125
// The board has BQ2512A as switching regulator and requires shutdown via shipping mode
#define HAS_I2C_BQ25121A
#define USE_BQ25121A
#define BATTERY_UVLO_SHUTDOWN
#define BATTERY_LIION

// Program BQ25121A to 2.5V output, 10mA charge current, 5mA precharge 
#define SYS_VOLTAGE 2500
#define CHARGE_CURRENT 10
#define PRECHARGE_CURRENT 5
#endif

// TX power settings (in dBm)
// Maximum power broadcast
#if defined(CONFIG_SOC_NRF52833) || defined(CONFIG_SOC_NRF54L15) || defined(CONFIG_SOC_NRF52840)
#define TX_POWER_HIGH 8
#else
#define TX_POWER_HIGH 4
#endif
// Normal power output
#define TX_POWER_NORMAL 0
// Low power output
#define TX_POWER_LOW -8

#ifdef BATTERY_LIION
// Li-Ion battery cutoff voltage (will switch to power save mode when it is reached)
// Ignored if BATTERY_UVLO_SHUTDOWN is not defined
#define BATTERY_LOW_VOLTAGE 2800

// Battery levels for Li-Ion batteries (in mV)
#define BATTERY_LEVEL_FULL 4000
#define BATTERY_LEVEL_NORMAL 3700
#define BATTERY_LEVEL_LOW 3300

#else

#define BATTERY_LOW_VOLTAGE 2400

// Battery levels for Lithium non-rechargeable cells (in mV)
#define BATTERY_LEVEL_FULL 3000
#define BATTERY_LEVEL_NORMAL 2900
#define BATTERY_LEVEL_LOW 2700
#endif

#endif
