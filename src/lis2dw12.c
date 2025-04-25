// LIS2DW12 accelerometer support
#include "lis2dw12.h"

#ifdef USE_LIS2DW12

#include <stdlib.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
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
#include "settings.h"

#define I2C0_NODE DT_NODELABEL(lis2dw12)
static const struct i2c_dt_spec dev_acc_i2c = I2C_DT_SPEC_GET(I2C0_NODE);

const struct device *const i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

int16_t accelTemperature = 0;

// Suspend i2c bus to save power
void lis2dw12_suspend_i2c() {
  int rc = pm_device_action_run(i2c_dev, PM_DEVICE_ACTION_SUSPEND);

  if (rc < 0)
    printk("Could not suspend I2C (%d)\n", rc);
  return;
}

// Resume i2c bus
void lis2dw12_resume_i2c() {
  int rc = pm_device_action_run(i2c_dev, PM_DEVICE_ACTION_RESUME);

  if (rc < 0)
    printk("Could not resume I2C (%d)\n", rc);
  return;
}

int16_t accSamples[33][3];

// Movements detected with 20 seconds interval
uint8_t moves20[30];
// Movements detected with 5 minutes interval
uint8_t moves300[288];

// When we did last shift for moves300 array
uint32_t lastMovesShift = 0;

// Read LIS2DW12 accelerometer fifo (all)
int accel_read() {
  int ret, i;
  int16_t accTemp;

  lis2dw12_resume_i2c();
  if (!device_is_ready(dev_acc_i2c.bus)) {
    printk("I2C bus %s is not ready!\n\r",dev_acc_i2c.bus->name);
    return -1;
  }

  ret = i2c_burst_read_dt(&dev_acc_i2c, LIS2DW12_REG_OUT_TEMP_L, (uint8_t *) &accTemp, 2);
  if (ret != 0) {
    printk("Failed to read from I2C device address %x at Reg. %x \n\r", dev_acc_i2c.addr, LIS2DW12_REG_OUT_TEMP_L);
    return -1;
  }
  accelTemperature = (int) (250 + ((float) accTemp / 25.6f));
  printk("LIS2DW12 temperature: %d (%04x) %d\n", accTemp, accTemp, accelTemperature);

  ret = i2c_burst_read_dt(&dev_acc_i2c, LIS2DW12_REG_OUT_X_L, ((uint8_t *) accSamples) + 6, sizeof(accSamples) - 6);
  if (ret != 0) {
    printk("Failed to read from I2C device address %x at Reg. %x \n\r", dev_acc_i2c.addr, LIS2DW12_REG_OUT_X_L);
    return -1;
  }

  lis2dw12_suspend_i2c();

  int first = 1;
  // Check if we have previous last reading in the buffer
  if ((accSamples[0][0] == -1) && (accSamples[0][1] == -1) && (accSamples[0][2] == -1))
    first = 2;

  // Calculate number of disturbances from accelerometer data
  int movements = 0;
  for (i = first; i < 33; i++) 
    if ((abs(accSamples[i-1][0] - accSamples[i][0]) > accelThreshold)
        || (abs(accSamples[i-1][1] - accSamples[i][1]) > accelThreshold)
        || (abs(accSamples[i-1][2] - accSamples[i][2]) > accelThreshold))
      movements++;

  printk("lis2dw12_read: movements %d\n", movements);

  // shift 20sec array
  memmove(moves20 + 1, moves20, sizeof(moves20) - 1);
  // Count as moving if we have more than one large shift in any of axis'es reading
  if (movements > 1)
    moves20[0] = 1;
  else
    moves20[0] = 0;

  // Check if we have to shift moves300 array (every 5 minutes)
  if (k_uptime_seconds() > (lastMovesShift + 300)) {
    memmove(moves300 + 1, moves300, sizeof(moves300) - 1);
    lastMovesShift = k_uptime_seconds();
  }  

  // Update first two elements of moves300 array
  moves300[0] = 0;
  moves300[1] = 0;
  for (i = 0; i < 15; i++) 
    if (moves20[i]) {
      moves300[0] = 1;
      break;
    }
  for (i = 15; i < 30; i++) 
    if (moves20[i]) {
      moves300[1] = 1;
      break;
    }

  // Put last reading onto first position for next cycle
  accSamples[0][0] = accSamples[32][0];
  accSamples[0][1] = accSamples[32][1];
  accSamples[0][2] = accSamples[32][2];
  return 0;
}

// Check for movements during a specific period
// startOffset, endOffset - time in minutes from current
// Data is stored in a circular buffer with 5 minutes resolution
int accel_movement(int startOffset, int endOffset) {
  int start = startOffset / 5;
  int end = (endOffset + 4)/ 5;

  if ((start < 0) || (start >= sizeof(moves300)))
    start = 0;
  if ((end < 0) || (end > sizeof(moves300)))
    end = sizeof(moves300);
  
  for (int i = start; i < end; i++)
    if (moves300[i] > 0)
      return 1;
  return 0;
}

// Power down accelerometer
int accel_powerdown() {
  lis2dw12_resume_i2c();
  if (!device_is_ready(dev_acc_i2c.bus)) {
    printk("I2C bus %s is not ready!\n\r",dev_acc_i2c.bus->name);
    return -1;
  }
  i2c_reg_write_byte_dt(&dev_acc_i2c, LIS2DW12_REG_CTRL_REG1, 0x0);
  accelTemperature = 0;

  lis2dw12_suspend_i2c();
  return 0;
}

static int arrays_initialized = 0;

// Init LIS2DW12 accelerometer
int accel_init() {
  int ret;
  uint8_t data;

  lis2dw12_resume_i2c();
  if (!device_is_ready(dev_acc_i2c.bus)) {
    printk("I2C bus %s is not ready!\n\r",dev_acc_i2c.bus->name);
    return -1;
  }

  i2c_reg_read_byte_dt(&dev_acc_i2c, LIS2DW12_REG_WHO_AM_I, &data);
  printk("LIS2DW12 init: read who_am_i reg: %02x\n", data);
  
  // Soft reset
  i2c_reg_write_byte_dt(&dev_acc_i2c, LIS2DW12_REG_CTRL_REG2, LIS2DW12_CTRL_REG2_SOFT_RESET);
  k_sleep(K_MSEC(1));
  // Load trimming
  i2c_reg_write_byte_dt(&dev_acc_i2c, LIS2DW12_REG_CTRL_REG2, LIS2DW12_CTRL_REG2_BOOT);
  k_sleep(K_MSEC(20));

  // Low power mode 1 1.65Hz Continous mode, no interrupts
  i2c_reg_write_byte_dt(&dev_acc_i2c, LIS2DW12_REG_CTRL_REG7, LIS2DW12_CTRL_REG7_DRDY_PULSES);
  i2c_reg_write_byte_dt(&dev_acc_i2c, LIS2DW12_REG_WAKE_UP_THS, 0x0);
  i2c_reg_write_byte_dt(&dev_acc_i2c, LIS2DW12_REG_WAKE_UP_DUR, 0x0);
  i2c_reg_write_byte_dt(&dev_acc_i2c, LIS2DW12_REG_FIFO_CTRL, 0xC0);
  i2c_reg_write_byte_dt(&dev_acc_i2c, LIS2DW12_REG_INT_DUR, 0x0);
  i2c_reg_write_byte_dt(&dev_acc_i2c, LIS2DW12_REG_CTRL_REG6, LIS2DW12_CTRL_REG6_LOW_NOISE);
  i2c_reg_write_byte_dt(&dev_acc_i2c, LIS2DW12_REG_CTRL_REG5, 0x0);
  i2c_reg_write_byte_dt(&dev_acc_i2c, LIS2DW12_REG_CTRL_REG4, 0x0);
  // Somewhat open-drain output on interrupts causes problems with high current consumption at some voltages, using push-pull
  i2c_reg_write_byte_dt(&dev_acc_i2c, LIS2DW12_REG_CTRL_REG3, 0x0);
  // KKM K4P has CS connected to VDD directly, no need pull-up
  i2c_reg_write_byte_dt(&dev_acc_i2c, LIS2DW12_REG_CTRL_REG2, LIS2DW12_CTRL_REG2_IF_ADD_INC | LIS2DW12_CTRL_REG2_CS_PU_DISC);
  // Low power mode 1 (12 bit), 1.65Hz ODR
  i2c_reg_write_byte_dt(&dev_acc_i2c, LIS2DW12_REG_CTRL_REG1, 0x10);
  
  lis2dw12_suspend_i2c();
  
  // Initialize arrays
  if (!arrays_initialized) {
    accSamples[0][0] = -1;
    accSamples[0][1] = -1;
    accSamples[0][2] = -1;
    memset(moves20, 0, sizeof(moves20));
    memset(moves300, 0, sizeof(moves300));
    arrays_initialized = 1;
  }
  return 0;
}


#endif
