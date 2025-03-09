/*
 * Copyright (c) 2018 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/sys/poweroff.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/controller.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/i2c.h>

#include <zephyr/sys/reboot.h>

#include "myboards.h"
#include "settings.h"
#include "watchdog.h"
#ifdef USE_BQ25121A
#include "bq2512x.h"
#endif

#define BROADCAST_INTERVAL_MIN 1500 // 938ms
#define BROADCAST_INTERVAL_MAX 1590 // 994ms

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
	!DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#warning "No suitable devicetree node, battery check disabled"
#define NO_BATTERY_CHECK
#endif

#ifndef NO_BATTERY_CHECK
#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
			     DT_SPEC_AND_COMMA)
};
#endif

#if CONFIG_GPIO
#define LED_NODE DT_NODELABEL(led0)
static const struct gpio_dt_spec led_spec = GPIO_DT_SPEC_GET(LED_NODE, gpios);
#define GPIO_ID DT_NODELABEL(gpio0)
#ifdef HAS_LED1_SWITCH
#define LED1_NODE DT_NODELABEL(led1)
static const struct gpio_dt_spec gate_spec = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
#endif
#endif

#ifdef HAS_I2C_KXTJ3
#define I2C0_NODE DT_NODELABEL(kxtj3)
static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(I2C0_NODE);
const struct device *const i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
#endif

void blink_led(int num) {
	int err;

#if CONFIG_GPIO
#if CONFIG_PM_DEVICE_RUNTIME
  	err = pm_device_action_run(DEVICE_DT_GET(DT_NODELABEL(gpio0)), PM_DEVICE_ACTION_RESUME);
	if (err)
		printk("Warning: Can't turn on gpio0 (err %d)\n", err);
#endif
	gpio_pin_configure_dt(&led_spec, GPIO_OUTPUT_INACTIVE);
	k_sleep(K_MSEC(1000));
	for (int i = 0; i < (2*num); i++) {
		gpio_pin_toggle_dt(&led_spec);
		k_sleep(K_MSEC(200));
	}
#if CONFIG_PM_DEVICE_RUNTIME
	gpio_pin_configure_dt(&led_spec, GPIO_DISCONNECTED);
	err = pm_device_action_run(DEVICE_DT_GET(DT_NODELABEL(gpio0)), PM_DEVICE_ACTION_SUSPEND);
	if (err)
		printk("Warning: Can't turn off gpio0 (err %d)\n", err);
#endif
#endif
}

int lastBatteryVoltage = 0;

#ifdef USE_BQ25121A
int check_charging() {
	return bq2512x_is_charging();
}

int battery_voltage(void) {
	return bq2512x_lastVoltage;
}
#else
int check_charging() {
	// There is no way to check if we're charging on boards without BQ25121A
	return 0;
}

int battery_voltage(void) {
#ifndef NO_BATTERY_CHECK
	int err;
	int32_t val_mv;
	int16_t buf = 0;
	uint8_t level = 0;
	int64_t sum_mv = 0;
	int i;

#if CONFIG_PM_DEVICE_RUNTIME
	err = pm_device_action_run(DEVICE_DT_GET(DT_NODELABEL(adc)), PM_DEVICE_ACTION_RESUME);
	if (err)
		printk("Warning: Can't turn on adc0 (err %d)\n", err);
#endif
	struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(buf),
	};

	if (!device_is_ready(adc_channels[0].dev)) {
		printk("ADC controller device not ready\n");
		return level;
	}
	err = adc_channel_setup_dt(&adc_channels[0]);
	if (err < 0) {
		printk("Could not setup channel #%d (%d)\n", 0, err);
		return level;
	}
	(void)adc_sequence_init_dt(&adc_channels[0], &sequence);

	// read adc 8 times to average
	for (i = 0; i < BATTERY_ADC_SAMPLES; i++) {
		err = adc_read(adc_channels[0].dev, &sequence);
		if (err < 0) {
			printk("Could not read (%d)\n", err);
		return level; 
		}

		val_mv = buf;
		err = adc_raw_to_millivolts_dt(&adc_channels[0], &val_mv);
		if (err < 0) {
			printk(" (value in mV not available)\n");
			return level; 
		}
		k_sleep(K_MSEC(5));
		sum_mv += val_mv;
	}
	val_mv = BATTERY_MULTIPLIER * (sum_mv / BATTERY_ADC_SAMPLES);

	printk("voltage level: %d\n", val_mv);
#if CONFIG_PM_DEVICE_RUNTIME
	err = pm_device_action_run(DEVICE_DT_GET(DT_NODELABEL(adc)), PM_DEVICE_ACTION_SUSPEND);
	if (err)
		printk("Warning: Can't turn off adc0 (err %d)\n", err);
#endif

  lastBatteryVoltage = val_mv;
	return val_mv;
#else
	return 0;
#endif
}
#endif


// airtag advertising data
struct bt_data adv_airtag[] = {
  BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA,
          0x4c, 0x00, 0x02, 0x15, 0x18, 0xee, 0x15, 0x16,
          0x01, 0x6b, 0x4b, 0xec, 0xad, 0x96, 
          0xbc, 0xb9, 0x6d, 0x16, 0x6e, 0x97, 
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  ),
};

// fmdn advertising data
struct bt_data adv_fmdn[] = {
  BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR | BT_LE_AD_GENERAL),
  BT_DATA_BYTES(
    0x16, 0xAA, 0xFE, 0x41,
    // public key
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // status
    0xc0
  ),
};

struct bt_data adv_ibeacon[] = {
  BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
  BT_DATA_BYTES(    0xff,       /* Manufacturer Specific Data (type 0xff) */
              0x4c, 0x00, /* Apple */
              0x02, 0x15, /* iBeacon */
              0x18, 0xee, 0x15, 0x16, /* UUID[15..12] */
              0x01, 0x6b, /* UUID[11..10] */
              0x4b, 0xec, /* UUID[9..8] */
              0xad, 0x96, /* UUID[7..6] */
              0xbc, 0xb9, 0x6d, 0x16, 0x6e, 0x97, /* UUID[5..0] */
              0x1, 0x1, /* Major */
              0x1, 0x1, /* Minor */ /* Hint (0x00) */
        0x0,
)
};

// MAC address (calculated from airtag key unless only FMDN broadcasting)
uint8_t bleAddr[6] = {0xFF, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

static uint8_t offline_finding_adv_template[] = {
	0x1e,		/* Length (30) */
	0xff,		/* Manufacturer Specific Data (type 0xff) */
	0x4c, 0x00, /* Company ID (Apple) */
	0x12, 0x19, /* Offline Finding type and length */
	0xAA,		/* Status */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, /* First two bits */
	0x00, /* Hint (0x00) */
};

uint8_t airtag_data_store[sizeof(offline_finding_adv_template)];
uint8_t fmdn_data_store[25];

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

static int keys_changes = 0;

// Update status byte in Airtag/FMDN advertisement broadcasts (see settings.h)
void update_status_byte() {
  uint8_t statusAirtag, statusFmdn;
  int bAirtag, bFmdn;

  statusAirtag = statusFlags & 0xff;
  statusFmdn = (statusFlags & 0xff00) >> 8;
  bAirtag = (statusFlags & 0xf0000) >> 16;
  bFmdn = (statusFlags & 0xf00000) >> 20;
  printk("update_status_byte: battery: %d Airtag %d:%02X, %d:%02X\n", lastBatteryVoltage, bAirtag, statusAirtag, bFmdn, statusFmdn);
  // Setting status for airtag
  switch (bAirtag) {
    case 1:
      // Broadcast first byte from statusFlags as is
      break;
    case 2:
      // Broadcast incrementing byte as status byte
      statusAirtag = keys_changes & 0xff;
      break;
    case 3:
      // Broadcast battery voltage in status byte
      statusAirtag = lastBatteryVoltage / 100;
      break;
    case 4:
      // Broadcast battery status
      int battery = 3;
      if (lastBatteryVoltage > BATTERY_LEVEL_FULL)
        battery = 0;
      else {
        if (lastBatteryVoltage > BATTERY_LEVEL_NORMAL)
          battery = 1;
        else {
          if (lastBatteryVoltage > BATTERY_LEVEL_LOW)
            battery = 2;
          else
            battery = 3;
        }
      }
      statusAirtag |= (battery << 6);
      break;
    default:
      break;
  }
  airtag_data_store[6] = statusAirtag;

  // Setting status for FMDN
  switch (bFmdn) {
    case 1:
      // Broadcast first byte from statusFlags as is
      break;
    case 2:
      // Broadcast incrementing byte as status byte
      statusFmdn = keys_changes & 0xff;
      break;
    case 3:
      // Broadcast battery voltage in status byte
      statusFmdn = lastBatteryVoltage / 100;
      break;
    case 4:
      // Broadcast battery status
      int battery = 0;
      if (lastBatteryVoltage > BATTERY_LEVEL_NORMAL)
        battery = 1;
      else {
        if (lastBatteryVoltage > BATTERY_LEVEL_LOW)
          battery = 2;
        else {
          if (lastBatteryVoltage > 0)
            battery = 3;
          else
            battery = 0;
        }
      }
      statusFmdn |= (battery << 5);
      break;
    default:
      break;
  }
  fmdn_data_store[23] = statusFmdn;
}

/*
 * set_addr_from_key will set the bluetooth address from the first 6 bytes of the key used to be advertised
 */
void set_addr_from_key(const char *key)
{
	/* copy first 6 bytes */
	bleAddr[5] = key[0] | 0b11000000;
	bleAddr[4] = key[1];
	bleAddr[3] = key[2];
	bleAddr[2] = key[3];
	bleAddr[1] = key[4];
	bleAddr[0] = key[5];
}

/*
 * fill_adv_template_from_key will set the advertising data based on the remaining bytes from the advertised key
 */
void fill_adv_template_from_key(const char *key)
{
	memcpy(&offline_finding_adv_template[7], &key[6], 22);
	/* append two bits of public key */
	offline_finding_adv_template[29] = key[0] >> 6;
}

/*
 * setAdvertisementKey will setup the key to be advertised
 *
 * @param[in] key public key to be advertised
 * @param[out] bleAddr bluetooth address to setup
 * @param[out] data raw data to advertise
 *
 * @returns raw data size
 */
uint8_t setAdvertisementKey(const char *key, uint8_t **data)
{
	set_addr_from_key(key);
	fill_adv_template_from_key(key);

	*data = airtag_data_store;
	memcpy(*data, offline_finding_adv_template, sizeof(offline_finding_adv_template));
	return sizeof(offline_finding_adv_template);
}

void prepare_airtag(const uint8_t *key) {
	uint8_t *data;

	setAdvertisementKey(key, &data);
	// Set right data for AirTag broadcast
	adv_airtag[0].data = data + 2;
}

static void set_tx_power(uint8_t handle_type, uint16_t handle, int8_t tx_pwr_lvl) {
  struct bt_hci_cp_vs_write_tx_power_level *cp;
  struct bt_hci_rp_vs_write_tx_power_level *rp;
  struct net_buf *buf, *rsp = NULL;
  int err;

  buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
        sizeof(*cp));
  if (!buf) {
    printk("Unable to allocate command buffer\n");
    return;
  }

  cp = net_buf_add(buf, sizeof(*cp));
  cp->handle = sys_cpu_to_le16(handle);
  cp->handle_type = handle_type;
  cp->tx_power_level = tx_pwr_lvl;

  err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
           buf, &rsp);
  if (err) {
    uint8_t reason = rsp ?
      ((struct bt_hci_rp_vs_write_tx_power_level *)
        rsp->data)->status : 0;
    printk("Set Tx power err: %d reason 0x%02x\n", err, reason);
    return;
  }

  rp = (void *)rsp->data;
  printk("Actual Tx Power: %d\n", rp->selected_tx_power);

  net_buf_unref(rsp);
}

// Set TX power for broadcasts, 0 - -8dbm, 1 - 0dbm, 2 - 4dbm
static void set_power(int power) {
	int pwrLevel;

	switch (power) {
		case 0:
			pwrLevel = TX_POWER_LOW;
			break;
		case 2:
			pwrLevel = TX_POWER_HIGH;
			break;
		default:
			pwrLevel = TX_POWER_NORMAL;
	}
	set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, pwrLevel);
}

void set_MAC_addr(uint8_t *addr) {
	bt_addr_le_t myaddr;

	myaddr.type = BT_ADDR_LE_PUBLIC;
	memcpy(myaddr.a.val, addr, sizeof(myaddr.a.val));
	bt_ctlr_set_public_addr(myaddr.a.val);
}

int broadcastingAnything = 0;
int broadcastingFmdn = 0;
int broadcastingAirtag = 0;
int broadcastingSettings = 0;

void broadcast() {
  int err = -1;

  // If we broadcast nothing, then just stop
  if (broadcastingAnything && !broadcastingFmdn && !broadcastingAirtag && !broadcastingSettings) {
    printk("Stopping broadcast\n");
    bt_le_adv_stop();
    broadcastingAnything = 0;
    return;
  }
  if (broadcastingSettings) {
    printk("We're still broadcasting settings!\n");
    return;
  }
  if (broadcastingAnything) {
    // Just update broadcast if we are broadcasting something
    printk("Updating broadcast: airtag %d, fmdn %d\n", broadcastingAirtag, broadcastingFmdn);
    if (broadcastingAirtag) {
	    err = bt_le_adv_update_data(adv_airtag, ARRAY_SIZE(adv_airtag), NULL, 0);
      broadcastingAnything = 1;
    }
    if (broadcastingFmdn) {
	    err = bt_le_adv_update_data(adv_fmdn, ARRAY_SIZE(adv_fmdn), NULL, 0);
      broadcastingAnything = 1;
    }
  } else {
    // Start broadcast
    printk("Starting broadcast: airtag %d, fmdn %d\n", broadcastingAirtag, broadcastingFmdn);
    if (broadcastingAirtag) {
      set_MAC_addr(bleAddr);
		  err = bt_le_adv_start(BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_IDENTITY, BROADCAST_INTERVAL_MIN * multPeriod, BROADCAST_INTERVAL_MAX * multPeriod, NULL),
                            adv_airtag, ARRAY_SIZE(adv_airtag), NULL, 0);
      broadcastingAnything = 1;
    }
    if (broadcastingFmdn) {
		  err = bt_le_adv_start(BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_IDENTITY, BROADCAST_INTERVAL_MIN * multPeriod, BROADCAST_INTERVAL_MAX * multPeriod, NULL),
                            adv_fmdn, ARRAY_SIZE(adv_fmdn), NULL, 0);
      broadcastingAnything = 1;
    }
  } 
	if (err) {
		printk("Advertising start/update failed (err %d)\n", err);
		return;
	}
}

uint8_t iBeacon_data[32];

// Start broadcasting iBeacon (in case no fmdn or airtag)
void broadcast_ibeacon() {
  int err = -1;

  printk("Starting broadcasting iBeacon\n");
  memcpy(iBeacon_data, adv_ibeacon[1].data, adv_ibeacon[1].data_len);
  adv_ibeacon[1].data = iBeacon_data;
  iBeacon_data[24] = (lastBatteryVoltage + 50) / 100;

  err = bt_le_adv_start(BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_IDENTITY, 11200, 12800, NULL),
                        adv_ibeacon, ARRAY_SIZE(adv_ibeacon), NULL, 0);
	if (err) {
		printk("Advertising start failed (err %d)\n", err);
		return;
	}
}

// Current airtag key broadcasted
int currentKey = 0;

// Switch Airtag key
void switch_airtag_key(int n) {
	if (n >= numKeys) {
		printk("Error at key switching, no key %d (total %d keys)\n", n, numKeys);
		return;
	}
	prepare_airtag(currentKeys[n]);
}

// Currently there is no support for multiple FMDN keys,
// so just prepare broadcast data for FMDN
void switch_fmdn_key(int n) {
  memcpy(fmdn_data_store, adv_fmdn[1].data, adv_fmdn[1].data_len);
  memcpy(fmdn_data_store + 3, fmdnKey, sizeof(fmdnKey));
  adv_fmdn[1].data = fmdn_data_store;
}

// Update battery voltage in iBeacon packet
void update_battery() {
	#ifdef USE_BQ25121A
	bq2512x_reinit(0, SYS_VOLTAGE, CHARGE_CURRENT, PRECHARGE_CURRENT);
	#endif
	lastBatteryVoltage = battery_voltage();
}

void reset_MCU() {
  printk("Reseting after changing settings or mcumgr (by WDT)\n");
  for (int i = 0; i < 8; i++) {
    store_time();
    k_sleep(K_MSEC(1000));
  }
	printk("wdt not reacted, resetting by sys_reboot\n");
	k_sleep(K_MSEC(1000));
  sys_reboot(SYS_REBOOT_COLD);
}

// If we detect charging in progress - don't shutdown BUVLO for hour
int chargeLockCounter = 0;

uint32_t tEndSettings = 0;
uint32_t tLastTimeSave = 0;
uint32_t tLastBatteryCheck = 0;
uint32_t tLastSettingsMode = 0;
uint32_t tLastMaxPower = 0;
uint32_t tEndMaxPower = 0;
uint32_t tLastAirtagSwitch = 0;

int main(void)
{
	int err;
	int badPower = 0;
	
	my_wdt_init();

	printk("Beacon starting\n");

	blink_led(1);

#ifdef HAS_LED1_SWITCH
	// Turn off mosfet
	gpio_pin_configure_dt(&gate_spec, GPIO_OUTPUT_ACTIVE);
#endif

	my_wdt_feed();
	init_settings();

#ifdef USE_BQ25121A
	// Repeat twice at start to be sure the bq25121a received settings
	bq2512x_reinit(1, SYS_VOLTAGE, CHARGE_CURRENT, PRECHARGE_CURRENT);
	k_sleep(K_MSEC(10));
	bq2512x_reinit(1, SYS_VOLTAGE, CHARGE_CURRENT, PRECHARGE_CURRENT);
#endif

  // Set FMDN advertising data
  switch_fmdn_key(0);

	printk("keys in memory = %d\n", numKeys);
  currentKey = 0;
	switch_airtag_key(currentKey);
  if (flagAirtag || flagFmdn)
    set_MAC_addr(bleAddr);
  broadcastingAnything = 0;
  broadcastingAirtag = 0;
  broadcastingFmdn = 0;
  broadcastingSettings = 0;

	update_battery();
  update_status_byte();

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	printk("Bluetooth initialized (err=%d)\n", err);
	
	set_power(txPower);
	my_wdt_feed();
	blink_led(2);

  // Broadcast ibeacon every 7..8 seconds if no airtag or fmdn
  if (!flagAirtag && !flagFmdn)
    broadcast_ibeacon();

	while (1) {
    // In settings mode we do only 2 seconds delay
    if (broadcastingSettings)
      k_sleep(K_MSEC(1000));
    else
      k_sleep(K_MSEC(1000 * multPeriod));
		my_wdt_feed();

		if (chargeLockCounter > 0) 
			chargeLockCounter = chargeLockCounter - multPeriod;

    // Check if we're in settings mode
    if (broadcastingSettings) {
      // Check if settings mode ended
      if (k_uptime_seconds() >= tEndSettings) {
        // Check if someone did connect, wait 2 seconds for auth
        if (connectedGatt || authorizedGatt) {
          k_sleep(K_MSEC(2000));
          my_wdt_feed();
        }
        if (authorizedGatt) {
          k_sleep(K_MSEC(4000));
          my_wdt_feed();
        }
        // Check if we have to wait for firmware upload
        if (pauseUpload) {
          printk("Pause for 60 seconds for firmware upload\n");
          for (int i = 0; i < 15; i++) {
            k_sleep(K_MSEC(4000));
            store_time();
            my_wdt_feed();
          }
          reset_MCU();
          break;
        }

        stop_settings_adv();
        broadcastingSettings = 0;
        broadcastingAnything = 0;
        broadcastingFmdn = 0;
        broadcastingAirtag = 0;
        // Set MAC address back
        bt_disable();
        if (flagAirtag || flagFmdn)
          set_MAC_addr(bleAddr);
        bt_enable(NULL);
        set_power(txPower);
        // Do we have to reset?
        if (needsReset) {
          reset_MCU();
          break;
        }
        // Broadcast ibeacon every 7..8 seconds if no airtag or fmdn
        if (!flagAirtag && !flagFmdn)
          broadcast_ibeacon();
      }

    } else {
      // Update time in NVS storage
      if (k_uptime_seconds() >= (tLastTimeSave + INTERVAL_TIMESAVE)) { 
        tLastTimeSave = k_uptime_seconds();
        store_time();
      }
      // Update battery and store time every minute
      if (k_uptime_seconds() >= (tLastBatteryCheck + INTERVAL_BATTERYCHECK)) {
        tLastBatteryCheck = k_uptime_seconds();
        update_battery();      
        // Update status byte in airtag and fmdn
        update_status_byte();
        if (flagAirtag || flagFmdn)
          broadcast();

				#ifdef BATTERY_UVLO_SHUTDOWN
				// On Li-Ion battery we have to monitor battery voltage and switch to power-save mode when it is too low
				if (battery_voltage() < BATTERY_LOW_VOLTAGE) {
					// Skip UVLO shutdown for one hour if charging is detected
					if (check_charging())
						chargeLockCounter = 3600;

          // Shutdown if we are not charging and battery voltage is too low for 5 minutes at least
					if (chargeLockCounter <= 0) {
						badPower++;
						if (badPower > 5) {
							printk("Battery voltage is too low! Shutting down!\n");
							bt_le_adv_stop();
							#ifdef USE_BQ25121A
							// Fully turn off the system through shipping mode
							bq2512x_shipmode();
							#endif
							sys_poweroff();
							// Never should reach here but who knows... Turning off broadcasts, do nothing.
							while (battery_voltage() < BATTERY_LOW_VOLTAGE) {
								update_battery();
								k_sleep(K_MSEC(4000));
								my_wdt_feed();
							}
              reset_MCU();
              break;
						}
					} else {
						printk("Charging in progress was detected, skipping UVLO shutdown\n");
						badPower = 0;
					}
				} else
					badPower = 0;
				#endif
      }
      
      // Switch to settings mode every minute for few seconds
      if (k_uptime_seconds() >= (tLastSettingsMode + INTERVAL_SETTINGS)) {
        tLastSettingsMode = k_uptime_seconds();
        tEndSettings = k_uptime_seconds() + SETTINGS_WAIT;
        bt_le_adv_stop();
        bt_disable();
        // We use other MAC and low power in settings mode
        if (settingsMAC[0] != 0 && settingsMAC[5] != 0)
          set_MAC_addr(settingsMAC);
        bt_enable(NULL);
        set_power(0);
        start_settings_adv();
        broadcastingSettings = 1;
        broadcastingAnything = 1;
        broadcastingFmdn = 0;
        broadcastingAirtag = 0;
        continue;
      }

      // Switch to maximum power every 68 seconds
      if ((txPower != 2) && (flagFmdn && flagAirtag) && (k_uptime_seconds() >= (tLastMaxPower + INTERVAL_MAXPOWER))) {
        tLastMaxPower = k_uptime_seconds();
        tEndMaxPower = k_uptime_seconds() + 2 * multPeriod;
        set_power(2);
      }

      // Return to normal power after two cycles
      if ((tEndMaxPower > 0) && (k_uptime_seconds() >= tEndMaxPower)) {
        tEndMaxPower = 0;
        set_power(txPower);
      }

      // Do we have to switch airtag keys?
      if (flagAirtag && (k_uptime_seconds() >= (tLastAirtagSwitch + changeInterval))) {
        tLastAirtagSwitch = k_uptime_seconds();
  			if (numKeys > 1) {
  				currentKey++;
          if (currentKey >= numKeys)
            currentKey = 0;
        }	else {
  				currentKey = 0;
        }
        keys_changes++;
        bt_le_adv_stop();
  			bt_disable();
        broadcastingAnything = 0;
  			printk("Switching to key %d\n", currentKey);
  			switch_airtag_key(currentKey);
        set_MAC_addr(bleAddr);
  			bt_enable(NULL);
  			set_power(txPower);
  			broadcast();
      }

      // Do we have even broadcast anything?
      if (!broadcastingAirtag && !broadcastingFmdn && (flagAirtag || flagFmdn)) {
        if (flagAirtag)
          broadcastingAirtag = 1;
        else
          broadcastingFmdn = 1;
        broadcast();
        continue;
      }

      // Do we have to switch between broadcasts?
      if (flagAirtag && flagFmdn) {
        if (broadcastingAirtag) {
          broadcastingAirtag = 0;
          broadcastingFmdn = 1;
        } else {
          broadcastingAirtag = 1;
          broadcastingFmdn = 0;
        }  
        broadcast();
        continue;
      }
    }
  }
	return 0;
}
