// Support functions for BQ2512X series I2C chargers
//
#include "bq2512x.h"

#ifdef USE_BQ25121A

#include <stdlib.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/drivers/i2c.h>




#define I2C0_NODE DT_NODELABEL(bq2512x)
static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(I2C0_NODE);


#if CONFIG_GPIO
#define CD_PIN_NODE DT_NODELABEL(bq2512x_cd)
static const struct gpio_dt_spec cdpin_spec = GPIO_DT_SPEC_GET(CD_PIN_NODE, gpios);
#define GPIO_ID DT_NODELABEL(gpio0)
#endif

// Last known voltage of the battery
int bq2512x_lastVoltage;

const struct device *const i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

// Suspend i2c bus to save power
void bq2512x_suspend_i2c() {
  int rc = pm_device_action_run(i2c_dev, PM_DEVICE_ACTION_SUSPEND);

  if (rc < 0)
    printk("Could not suspend I2C (%d)\n", rc);
  return;
}

// Resume i2c bus
void bq2512x_resume_i2c() {
  int rc = pm_device_action_run(i2c_dev, PM_DEVICE_ACTION_RESUME);

  if (rc < 0)
    printk("Could not resume I2C (%d)\n", rc);
  return;
}

// Put into active mode (CD to high)
void bq2512x_activate() {
  printk("bq2512x_activate: pulling CD high\n");
  // active state is high
  gpio_pin_configure_dt(&cdpin_spec, GPIO_OUTPUT | GPIO_OUTPUT_ACTIVE);
}

// Put into high-impedance mode (CD to low)
void bq2512x_deactivate() {
  printk("bq2512x_deactivate: disconnecting CD\n");
  // Just deactivate GPIO line, CD is pulled down internally
  gpio_pin_configure_dt(&cdpin_spec, GPIO_DISCONNECTED);
}

// Set LDO output voltage in millivolts (if zero - LS/LDO will be disabled, default is 1.8V)
void bq2512x_set_ldo(int voltage) {
  int ldoval;

  if (!device_is_ready(dev_i2c.bus)) {
    printk("I2C bus %s is not ready!\n\r",dev_i2c.bus->name);
    return;
  }

  switch (voltage) {
    case 2800:
      ldoval = 0xF0;
      break;
    case 2700:
      ldoval = 0xEC;
      break;
    case 2500:
      ldoval = 0xE4;
      break;
    case 2300:
      ldoval = 0xDC;
      break;
    case 0:
      ldoval = 0;
      break;
    default:
      ldoval = 0xC8;
  }
  printk("bq2512x_set_ldo: Writing %02X to LOADSW_LDO_CTL\n", ldoval);
  int ret = i2c_reg_write_byte_dt(&dev_i2c, BQ2512X_LOADSW_LDO_CTL_REG, ldoval);
  if (ret != 0) {
    printk("bq2512x_set_ldo: write error %d!\n", ret);
    return;
  }
}

// Set SYS output voltage in millivolts (2300, 2500, 2700, 2800 are supported, all other values will cause 1.8V)
void bq2512x_set_sys(int voltage) {
  int sysval;

  if (!device_is_ready(dev_i2c.bus)) {
    printk("I2C bus %s is not ready!\n\r",dev_i2c.bus->name);
    return;
  }
  
  switch (voltage) {
    case 2800:
      sysval = 0xbe;
      break;
    case 2700:
      sysval = 0xbc;
      break;
    case 2500:
      sysval = 0xb8;
      break;
    case 2300:
      sysval = 0xb4;
      break;
    default:
      sysval = 0xaa;
  }
  printk("bq2512x_set_sys: Writing %02X to SYSTEM_VOUT_CTL\n", sysval);
  int ret = i2c_reg_write_byte_dt(&dev_i2c, BQ2512X_SYSTEM_VOUT_CTL_REG, sysval);
  if (ret != 0) {
    printk("bq2512x_set_sys: write error %d!\n", ret);
    return;
  }
}

// Set fast charge limit current (5/10/20/40mA)
void bq2512x_set_fchg(int current) {
  uint8_t fchg;

  if (!device_is_ready(dev_i2c.bus)) {
    printk("I2C bus %s is not ready!\n\r",dev_i2c.bus->name);
    return;
  }

  switch (current) {
    case 40:
      fchg = 0x90;
      break;
    case 20:
      fchg = 0x50;
      break;
    case 10:
      fchg = 0x28;
      break;
    default:
      // 5mA default
      fchg = 0x14;
  }
  printk("bq2512x_set_fchg: Writing %02X to FASTCHARGE_CTL\n", fchg);
  int ret = i2c_reg_write_byte_dt(&dev_i2c, BQ2512X_FASTCHARGE_CTL_REG, fchg);
  if (ret != 0) {
    printk("bq2512x_set_fchg: write error %d!\n", ret);
    return;
  }
}

// Set pre-charge/termination limit current (2/5/10/20mA)
void bq2512x_set_pchg(int current) {
  uint8_t pchg;

  if (!device_is_ready(dev_i2c.bus)) {
    printk("I2C bus %s is not ready!\n\r",dev_i2c.bus->name);
    return;
  }

  switch (current) {
    case 20:
      pchg = 0xba;
      break;
    case 10:
      pchg = 0x92;
      break;
    case 5:
      pchg = 0x2a;
      break;
    default:
      // 2mA default
      pchg = 0x12;
  }
  printk("bq2512x_set_pchg: Writing %02X to CHARGETERM_CTL\n", pchg);
  int ret = i2c_reg_write_byte_dt(&dev_i2c, BQ2512X_CHARGETERM_CTL_REG, pchg);
  if (ret != 0) {
    printk("bq2512x_set_pchg: write error %d!\n", ret);
    return;
  }
}

// Disable TS control, returns zero if TS was disabled, positive if enabled, -1 if error at read
int bq2512x_ts_disable() {
  uint8_t data;
  int ret;
  int res = 1;
  int read_loops = 2;

  if (!device_is_ready(dev_i2c.bus)) {
    printk("I2C bus %s is not ready!\n\r",dev_i2c.bus->name);
    return -1;
  }
  do {
    ret = i2c_reg_read_byte_dt(&dev_i2c, BQ2512X_TSCONTROL_STATUS_REG, &data);
    if (ret != 0)
      printk("Failed to read from I2C device address %x at Reg. %x \n", dev_i2c.addr, BQ2512X_TSCONTROL_STATUS_REG);
    read_loops--;
  } while (read_loops > 0);
  if (ret != 0)
    return -1;

  // Check if TS is enabled
  if (data &0x80)
    res = 1;
  else 
    res = 0;
  printk("bq2512x_ts_disable: read %02X from TSCONTROL_STATUS_REG\n", data);
  data = 0x08;
  printk("bq2512x_ts_disable: Writing %02X to TSCONTROL_STATUS_REG\n", data);
  ret = i2c_reg_write_byte_dt(&dev_i2c, BQ2512X_TSCONTROL_STATUS_REG, data);
  if (ret != 0) {
    printk("bq2512x_ts_disable: write error %d!\n", ret);
    return -1;
  }
  return res;
}

// Set Battery under-voltage, in millivolts (3000, 2800, 2600, 2400 are supported)
void bq2512x_set_buvlo(int voltage) {
  int val;

  if (!device_is_ready(dev_i2c.bus)) {
    printk("I2C bus %s is not ready!\n\r",dev_i2c.bus->name);
    return;
  }
  switch (voltage) {
    case 3000:
      val = 0x12;
      break;
    case 2800:
      val = 0x13;
      break;
    case 2600:
      val = 0x14;
      break;
    default:
      val = 0x15;
  }
  printk("bq2512x_set_buvlo: Writing %02X to ILIMIT_UVLO_CTL_REG\n", val);
  int ret = i2c_reg_write_byte_dt(&dev_i2c, BQ2512X_ILIMIT_UVLO_CTL_REG, val);
  if (ret != 0) {
    printk("bq2512x_set_buvlo: write error %d!\n", ret);
    return;
  }
}

// Read status/shipmode register
int bq2512x_status() {
  int ret;
  uint8_t data;

  if (!device_is_ready(dev_i2c.bus)) {
    printk("I2C bus %s is not ready!\n\r",dev_i2c.bus->name);
    return -1;
  }
  ret = i2c_reg_read_byte_dt(&dev_i2c, BQ2512X_STATUS_SHIPMODE_REG, &data);
  if (ret != 0) {
    printk("Failed to read from I2C device address %x at Reg. %x \n\r", dev_i2c.addr, BQ2512X_STATUS_SHIPMODE_REG);
    return -1;
  }
  printk("bq2512x_status: read status/shipmode reg: %02x\n",data);
  return data;
}

// Switch to ship-mode
void bq2512x_shipmode() {
  bq2512x_resume_i2c();
  if (!device_is_ready(dev_i2c.bus)) {
    printk("I2C bus %s is not ready!\n\r",dev_i2c.bus->name);
    return;
  }
  // Check if we are charging (don't do shipmode if charging)
  int status = bq2512x_status();
  if ((status > 0) && (status & 0xC0))
    return;
  printk("bq2512x_shipmode: Attempting to enter shipmode\n");
  // Activate i2c and wait for transition time
  bq2512x_activate();
  k_sleep(K_MSEC(2));
  // Enter shipmode
  int ret = i2c_reg_write_byte_dt(&dev_i2c, BQ2512X_STATUS_SHIPMODE_REG, 0x20);
  if (ret != 0)
    printk("Failed to write to I2C device address %x at Reg. %x \n\r", dev_i2c.addr, BQ2512X_STATUS_SHIPMODE_REG);
  // Shouldn't be here but well...
  bq2512x_deactivate();
  bq2512x_suspend_i2c();
}

// Read faults register
int bq2512x_faults() {
  int ret;
  uint8_t data;

  if (!device_is_ready(dev_i2c.bus)) {
    printk("I2C bus %s is not ready!\n\r",dev_i2c.bus->name);
    return -1;
  }
  ret = i2c_reg_read_byte_dt(&dev_i2c, BQ2512X_FAULTS_FAULTMASKS_REG, &data);
  if (ret != 0) {
    printk("Failed to read from I2C device address %x at Reg. %x \n\r", dev_i2c.addr, BQ2512X_FAULTS_FAULTMASKS_REG);
    return -1;
  }
  printk("bq2512x_faults: read faults/masks reg: %02x\n",data);
  return data;
}

// Read power gauge register and return approximate voltage in millivolts
int bq2512x_gauge() {
  uint8_t data;
  int ret;

  if (!device_is_ready(dev_i2c.bus)) {
    printk("I2C bus %s is not ready!\n\r",dev_i2c.bus->name);
    return -1;
  }
  
  ret = i2c_reg_read_byte_dt(&dev_i2c, BQ2512X_BATT_VOLT_MONITOR_REG, &data);
  if (ret != 0) {
    printk("Failed to read from I2C device address %x at Reg. %x \n\r", dev_i2c.addr, BQ2512X_BATT_VOLT_MONITOR_REG);
    return -1;
  }

  // Initiate new battery reading
  ret = i2c_reg_write_byte_dt(&dev_i2c, BQ2512X_BATT_VOLT_MONITOR_REG, 0x80);
  if (ret != 0) {
    printk("Failed to write to I2C device address %x at Reg. %x \n\r", dev_i2c.addr, BQ2512X_BATT_VOLT_MONITOR_REG);
    return 0;
  }

  int range = ((data & 0x60)>> 5) * 10 + 60;
  int th = 0;
  switch ((data & 0x1C) >> 2) {
    case 7:
      th = 9;
      break;
    case 6:
      th = 7;
      break;
    case 3:
      th = 5;
      break;
    case 2:
      th = 3;
      break;
    case 1:
      th = 1;
  }
  int percent = range + th;
  int voltage = (4200 * percent) / 100;
  printk("bq2512x_gauge: read voltage monitor reg: %02x, percents: %d%%, voltage: %dmV\n",data, percent, voltage);
  return voltage;
}

// Re-initialize BQ2512x to appropriate parameters (if TS is enabled or forceFlag is not zero, otherwise just do nothing)
// sysVoltage in mv, fastCharge and preCharge currents in mA
// Also updates bq2512x_lastVoltage variable
void bq2512x_reinit(int forceFlag, int sysVoltage, int fastCharge, int preCharge) {
  int ret;
  int nAttempts = 3;

  bq2512x_resume_i2c();
  // Attempt to read and change TS flag
  ret = bq2512x_ts_disable();

  if (ret < 0) {
    // Got problem at read/write, pulling up CD to enable
    printk("bq2512x_reinit: couldn't read, pulling up CD\n");
    bq2512x_activate();
    // Wait for transition timer from High-Z to active
    k_sleep(K_MSEC(2));
    while ((ret = bq2512x_ts_disable()) < 0) {
      k_sleep(K_MSEC(10));
      nAttempts--;
    }
  }
  int voltage = bq2512x_gauge();
  if (voltage > 0)
    bq2512x_lastVoltage = voltage;
  printk("bq2512x_reinit: ret %d, forceFlag %d, bq2512x_lastVoltage %d\n", ret, forceFlag, bq2512x_lastVoltage);
  // Read flags to reset faults
  (void) bq2512x_faults();
  if (ret || forceFlag) {        
        bq2512x_set_buvlo(2600);
        bq2512x_set_ldo(0);
        bq2512x_set_sys(sysVoltage);
        bq2512x_set_fchg(fastCharge);
        bq2512x_set_pchg(preCharge);
        // Read flags to reset faults
        (void) bq2512x_faults();
        (void) bq2512x_status();
  }

  // Switching to High-Z
  bq2512x_deactivate();
  bq2512x_suspend_i2c();
}

// Check if we're in charging mode (returns 0 if not charging, 1 if charging)
int bq2512x_is_charging() {
  int ret;
  bq2512x_resume_i2c();
  // Attempt to read and change TS flag
  ret = bq2512x_ts_disable();
  // If we couldn't read anything without pulling up CD line - we aren't in charge mode for sure
  if (ret < 0)
    return 0;

  if ((bq2512x_status() & 0xc0) == 0x40)
    return 1;
  return 0;
}

#endif
