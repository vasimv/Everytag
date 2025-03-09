// Header file for boards with BQ25121A
#ifndef _BQ2512X_H
#define _BQ2512X_H

#include "myboards.h"

#ifdef USE_BQ25121A

/* Registers */
#define BQ2512X_STATUS_SHIPMODE_REG		0x00
#define BQ2512X_FAULTS_FAULTMASKS_REG		0x01
#define BQ2512X_TSCONTROL_STATUS_REG		0x02
#define BQ2512X_FASTCHARGE_CTL_REG		0x03
#define BQ2512X_CHARGETERM_CTL_REG		0x04
#define BQ2512X_BATT_VOLTAGE_CTL_REG		0x05
#define BQ2512X_SYSTEM_VOUT_CTL_REG		0x06
#define BQ2512X_LOADSW_LDO_CTL_REG		0x07
#define BQ2512X_PUSH_BTN_CTL_REG		0x08
#define BQ2512X_ILIMIT_UVLO_CTL_REG		0x09
#define BQ2512X_BATT_VOLT_MONITOR_REG		0x0A
#define BQ2512X_VIN_DPM_TIMER_REG		0x0B


// Last voltage read from bq2512x
extern int bq2512x_lastVoltage;

// Re-initialize BQ2512x to appropriate parameters (if TS is enabled or forceFlag is not zero, otherwise just do nothing)
// sysVoltage in mv, fastCharge and preCharge currents in mA
// Also updates bq2512x_lastVoltage variable
void bq2512x_reinit(int forceFlag, int sysVoltage, int fastCharge, int preCharge);

// Enter shipping mode (turns off everything to save power, will activate when charging)
void bq2512x_shipmode();

// Check if we're in charging mode (returns 0 if not charging, 1 if charging)
int bq2512x_is_charging();

#endif

#endif
