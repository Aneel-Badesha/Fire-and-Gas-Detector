// Sensor reading functions and ADC configuration for all analog inputs

#ifndef SENSOR_H_
#define SENSOR_H_

#include <stdint.h>
#include "common.h"

// sysfs IIO paths for each ADC channel on the BeagleBone
#define A2D_FILE_VOLTAGE0 "/sys/bus/iio/devices/iio:device0/in_voltage0_raw"        // Temperature
#define A2D_FILE_VOLTAGE1 "/sys/bus/iio/devices/iio:device0/in_voltage1_raw"        // IR
#define A2D_FILE_VOLTAGE2 "/sys/bus/iio/devices/iio:device0/in_voltage2_raw"        // CO2
#define A2D_FILE_VOLTAGE3 "/sys/bus/iio/devices/iio:device0/in_voltage3_raw"        // Smoke
#define A2D_FILE_VOLTAGE5 "/sys/bus/iio/devices/iio:device0/in_voltage5_raw"        // CO

#define A2D_VOLTAGE_REF_V 1.8f   // ADC reference voltage (V)
#define A2D_MAX_READING   4095   // Maximum 12-bit ADC count
#define A2D_MIN_READING   0      // Minimum ADC count

// Sensor specific bounds
#define SENSOR_MIN_TEMP   50
#define SENSOR_MAX_TEMP   3800
#define SENSOR_MIN_IR     10
#define SENSOR_MAX_IR     4000
#define SENSOR_MIN_CO     50 
#define SENSOR_MAX_CO     4000
#define SENSOR_MIN_CO2    50
#define SENSOR_MAX_CO2    4000
#define SENSOR_MIN_SMOKE  50
#define SENSOR_MAX_SMOKE  4000

// Sensor config data struct
typedef struct {
    const char    *adc_path;        // IIO path for this sensor
    uint32_t       min_raw;         // ADC lower bound
    uint32_t       max_raw;         // ADC upper bound
    sensor_idx_t   sensor_idx;      // index into thread_data.value and g_mutex_sensor
    float         *ema_out;         // pointer to output field in thread_data
    int            watchdog_idx;    // index into watchdog slot to kick on a valid read
    long long      poll_ms;         // sleep duration between ADC reads
} sensor_cfg_t;

// Returns true if raw ADC count is within the sensor's valid operating range
static inline bool sensorReadValid(uint32_t raw, uint32_t min, uint32_t max) {
    return raw >= min && raw <= max;
}

// Reads a raw ADC count from the given sysfs path, returns false on failure
bool getVoltageReading(const char *path, uint32_t *out_value);

// Reads temperature sensor, applies EMA, updates value_temp
void *readTemperature(void *arg);

// Reads TCRT5000 IR sensor (50ms), applies EMA, updates value[SENSOR_IR]
void *readIR(void *arg);

// Reads CO sensor, applies EMA, updates value_co
void *readCO(void *arg);

// Reads CO2 sensor, applies EMA, updates value_co2
void *readCO2(void *arg);

// Reads smoke sensor, applies EMA, updates value_smoke
void *readSmoke(void *arg);

#endif
