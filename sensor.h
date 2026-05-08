// Header file for sensor functions

#ifndef SENSOR_H_
#define SENSOR_H_

#include "common.h"

#define A2D_FILE_VOLTAGE0 "/sys/bus/iio/devices/iio:device0/in_voltage0_raw"
#define A2D_FILE_VOLTAGE1 "/sys/bus/iio/devices/iio:device0/in_voltage1_raw"
#define A2D_FILE_VOLTAGE2 "/sys/bus/iio/devices/iio:device0/in_voltage2_raw"
#define A2D_FILE_VOLTAGE3 "/sys/bus/iio/devices/iio:device0/in_voltage3_raw"
#define A2D_FILE_VOLTAGE4 "/sys/bus/iio/devices/iio:device0/in_voltage4_raw"
#define A2D_FILE_VOLTAGE5 "/sys/bus/iio/devices/iio:device0/in_voltage5_raw"
#define A2D_FILE_VOLTAGE6 "/sys/bus/iio/devices/iio:device0/in_voltage6_raw"

#define A2D_VOLTAGE_REF_V 1.8
#define A2D_MAX_READING 4095

bool getVoltageReading(const char *path, int *out_value);

void *readTemperature(void *arg);

void *readIR(void *arg);

void *readAirSensors(void *arg);

#endif