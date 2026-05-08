// Header file for output/logic functions

#ifndef OUTPUT_H_
#define OUTPUT_H_

#include "common.h"

#define I2CDRV_LINUX_BUS1 "/dev/i2c-1"
#define I2C_DEVICE_ADDRESS 0x70

// LED-matrix command registers
#define LED_REG_SYSTEM_SETUP  0x21
#define LED_REG_DISPLAY_SETUP 0x81

// 8 row register addresses
#define LED_REG_ROW0 0x00
#define LED_REG_ROW1 0x02
#define LED_REG_ROW2 0x04
#define LED_REG_ROW3 0x06
#define LED_REG_ROW4 0x08
#define LED_REG_ROW5 0x0A
#define LED_REG_ROW6 0x0C
#define LED_REG_ROW7 0x0E

#define LED_PATTERN_OFF 0x00
#define LED_PATTERN_FULL 0xFF
#define LED_PATTERN_EXCLAMATION 0x0C

int i2cOpenBus(const char *bus_path, int slave_addr);

int i2cWriteCmd(int fd, unsigned char cmd);

int i2cWriteReg(int fd, unsigned char reg, unsigned char value);

void *calculateStatus(void *arg);

void *calcAlarm(void *arg);

void *displayOutput(void *arg);

#endif