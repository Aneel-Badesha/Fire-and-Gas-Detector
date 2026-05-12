// LED matrix output, alarm logic, and sensor status evaluation

#ifndef OUTPUT_H_
#define OUTPUT_H_

#include "common.h"

#define I2CDRV_LINUX_BUS1  "/dev/i2c-1"     // I2C bus device for the LED matrix
#define I2C_DEVICE_ADDRESS 0x70             // HT16K33 LED matrix I2C address

// HT16K33 command registers
#define LED_REG_SYSTEM_SETUP  0x21          // Turn on the oscillator
#define LED_REG_DISPLAY_SETUP 0x81          // Enable display output

// Row register addresses
// Each controls one row of the 8x8 matrix
#define LED_REG_ROW0 0x00
#define LED_REG_ROW1 0x02
#define LED_REG_ROW2 0x04
#define LED_REG_ROW3 0x06
#define LED_REG_ROW4 0x08
#define LED_REG_ROW5 0x0A
#define LED_REG_ROW6 0x0C
#define LED_REG_ROW7 0x0E

#define LED_PATTERN_OFF         0x00  // All LEDs off
#define LED_PATTERN_FULL        0xFF  // All LEDs on
#define LED_PATTERN_EXCLAMATION 0x0C  // Single column on

// Opens the I2C bus and sets the device address, returns fd or -1 on failure
int i2cOpenBus(const char *bus_path, int device_addr);

// Writes a single-byte command to the I2C device
int i2cWriteCmd(int fd, unsigned char cmd);

// Writes a value to a specific register on the I2C device
int i2cWriteReg(int fd, unsigned char reg, unsigned char value);

// Evaluates thresholds, drives LED matrix, and prints alarm/warning messages
void *calcAlarm(void *arg);

// Prints live sensor readings to stdout
void *displayOutput(void *arg);

#endif
