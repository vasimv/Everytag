// Header file for LIS2DW12 acelerometer
#ifndef _LIS2DW12_H
#define _LIS2DW12_H

#include <stdint.h>
#include "myboards.h"

#ifdef USE_LIS2DW12

// Accelerometer read interval (in seconds)
#define INTERVAL_ACCELEROMETER 20

#define LIS2DW12_ID                          0x44

#define LIS2DW12_REG_OUT_TEMP_L              0x0D
#define LIS2DW12_REG_OUT_TEMP_H              0x0E

#define LIS2DW12_REG_WHO_AM_I                0x0F

#define LIS2DW12_REG_CTRL_REG1               0x20
#define LIS2DW12_CTRL_REG1_ODR         (0xf << 4)
#define LIS2DW12_CTRL_REG1_MODE        (0x3 << 2)
#define LIS2DW12_CTRL_REG1_LP_MODE     (0x3 << 0)

#define LIS2DW12_REG_CTRL_REG2               0x21
#define LIS2DW12_CTRL_REG2_BOOT          (1 << 7)
#define LIS2DW12_CTRL_REG2_SOFT_RESET    (1 << 6)
#define LIS2DW12_CTRL_REG2_CS_PU_DISC    (1 << 4)
#define LIS2DW12_CTRL_REG2_BDU           (1 << 3)
#define LIS2DW12_CTRL_REG2_IF_ADD_INC    (1 << 2)
#define LIS2DW12_CTRL_REG2_I2C_DISABLE   (1 << 1)
#define LIS2DW12_CTRL_REG2_SIM           (1 << 0)

#define LIS2DW12_REG_CTRL_REG3               0x22
#define LIS2DW12_CTRL_REG3_ST_MODE     (0x3 << 6)
#define LIS2DW12_CTRL_REG3_PP_OD         (1 << 5)
#define LIS2DW12_CTRL_REG3_LIR           (1 << 4)
#define LIS2DW12_CTRL_REG3_H_LACTIVE     (1 << 3)
#define LIS2DW12_CTRL_REG3_SLP_MODE_SEL  (1 << 1)
#define LIS2DW12_CTRL_REG3_SLP_MODE_1    (1 << 0)
    
#define LIS2DW12_REG_CTRL_REG4               0x23

#define LIS2DW12_REG_CTRL_REG5               0x24
    
#define LIS2DW12_REG_CTRL_REG6               0x25
#define LIS2DW12_CTRL_REG6_BW_FILT     (0x3 << 6)
#define LIS2DW12_CTRL_REG6_FS          (0x3 << 4)
#define LIS2DW12_CTRL_REG6_FDS           (1 << 3)
#define LIS2DW12_CTRL_REG6_LOW_NOISE     (1 << 2)

#define LIS2DW12_REG_TEMP_OUT                0x26

#define LIS2DW12_REG_STATUS_REG              0x27


#define LIS2DW12_REG_OUT_X_L                 0x28
#define LIS2DW12_REG_OUT_X_H                 0x29
#define LIS2DW12_REG_OUT_Y_L                 0x2A
#define LIS2DW12_REG_OUT_Y_H                 0x2B
#define LIS2DW12_REG_OUT_Z_L                 0x2C
#define LIS2DW12_REG_OUT_Z_H                 0x2D

#define LIS2DW12_REG_FIFO_CTRL               0x2E
#define LIS2DW12_FIFO_CTRL_FMODE       (0x7 << 5)
#define LIS2DW12_FIFO_CTRL_FTH             (0x1F)

#define LIS2DW12_REG_FIFO_SAMPLES            0x2F
#define LIS2DW12_FIFO_SAMPLES_FTH        (1 << 7)
#define LIS2DW12_FIFO_SAMPLES_OVR        (1 << 6)
#define LIS2DW12_FIFO_SAMPLES              (0x3F)
    
#define LIS2DW12_REG_TAP_THS_X               0x30
#define LIS2DW12_TAP_THS_X_4D_EN         (1 << 7)
#define LIS2DW12_TAP_THS_X_6D_THS      (0x3 << 5)
#define LIS2DW12_TAP_THS_X_THS             (0x1F)

#define LIS2DW12_REG_TAP_THS_Y               0x31
#define LIS2DW12_TAP_THS_Y_PRIOR       (0x7 << 5)
#define LIS2DW12_TAP_THS_Y_THS             (0x1F)

#define LIS2DW12_REG_TAP_THS_Z               0x32
#define LIS2DW12_TAP_THS_Z_X_EN          (1 << 7)
#define LIS2DW12_TAP_THS_Z_Y_EN          (1 << 6)
#define LIS2DW12_TAP_THS_Z_Z_EN          (1 << 5)
#define LIS2DW12_TAP_THS_Z_THS             (0x1F)

#define LIS2DW12_REG_INT_DUR                 0x33
#define LIS2DW12_INT_DUR_LATENCY       (0xf << 4)
#define LIS2DW12_INT_DUR_QUIET         (0x3 << 2)
#define LIS2DW12_INT_DUR_SHOCK         (0x3 << 0)

#define LIS2DW12_REG_WAKE_UP_THS             0x34
#define LIS2DW12_WAKE_THS_SINGLE_DOUBLE_TAP  0x80
#define LIS2DW12_WAKE_THS_SLEEP_ON           0x40
#define LIS2DW12_WAKE_THS_THS                0x3F

#define LIS2DW12_REG_WAKE_UP_DUR             0x35
#define LIS2DW12_WAKE_DUR_FF_DUR             0x80
#define LIS2DW12_WAKE_DUR_DUR                0x60
#define LIS2DW12_WAKE_DUR_STATIONARY         0x10
#define LIS2DW12_WAKE_DUR_SLEEP_DUR          0x0F
    
#define LIS2DW12_REG_FREEFALL                0x36
#define LIS2DW12_FREEFALL_DUR         (0x1F << 3)
#define LIS2DW12_FREEFALL_THS          (0x7 << 0)

#define LIS2DW12_REG_WAKE_UP_SRC             0x38
#define LIS2DW12_REG_TAP_SRC                 0x39
#define LIS2DW12_REG_SIXD_SRC                0x3A
    
#define LIS2DW12_REG_INT_SRC                 0x3B

#define LIS2DW12_REG_X_OFS                   0x3C
#define LIS2DW12_REG_Y_OFS                   0x3D
#define LIS2DW12_REG_Z_OFS                   0x3E

#define LIS2DW12_REG_CTRL_REG7               0x3F
#define LIS2DW12_CTRL_REG7_DRDY_PULSES   (1 << 7)
#define LIS2DW12_CTRL_REG7_INT2_ON_INT1  (1 << 6)
#define LIS2DW12_CTRL_REG7_INT_EN        (1 << 5)
#define LIS2DW12_CTRL_REG7_USR_OFF_OUT   (1 << 4)
#define LIS2DW12_CTRL_REG7_USR_OFF_WU    (1 << 3)
#define LIS2DW12_CTRL_REG7_USR_OFF_W     (1 << 2)
#define LIS2DW12_CTRL_REG7_HP_REF        (1 << 1)
#define LIS2DW12_CTRL_REG7_LPASS_6D      (1 << 0)  

// Temperature*10 updated by lis2dw12_read()
// for example, 281 = 28.1C degrees
extern int16_t accelTemperature;

// Power down accelerometer
int accel_powerdown();

// Check for movements during a specific period
// startOffset, endOffset - time in minutes from current
// Data is stored in a circular buffer with 5 minutes resolution
int accel_movement(int startOffset, int endOffset);

// Init LIS2DW12 accelerometer
int accel_init();

// Read temperature and FIFO buffer LIS2DW12 accelerometer
// Also updates internal movement time table
int accel_read();

#endif
#endif
