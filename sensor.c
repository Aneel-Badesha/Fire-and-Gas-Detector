#include <inttypes.h>
#include "sensor.h"

// Channel 0 = Temperature (TMP36)
// Channel 1 = IR          (TCRT5000)
// Channel 2 = CO2         (MQ-135)
// Channel 3 = Smoke       (MQ-2)
// Channel 5 = CO          (MQ-7)

// Reads a raw 12-bit ADC count from the sysfs IIO path, returns false on failure
bool getVoltageReading(const char *path, uint32_t *out_value)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "WARN: unable to open %s; reusing previous reading\n", path);
        return false;
    }

    uint32_t a2d_reading = 0;
    int items_read = fscanf(f, "%" SCNu32, &a2d_reading);
    fclose(f);

    if (items_read <= 0) {
        fprintf(stderr, "WARN: unable to read from %s; reusing previous reading\n", path);
        return false;
    }

    *out_value = a2d_reading;
    return true;
}

// Converts a raw ADC count to Celsius using the TMP36 transfer function
float calcVoltageTemp(uint32_t voltage)
{
    float result = A2D_VOLTAGE_REF_V * ((float)voltage / (float)A2D_MAX_READING);
    float temperature = (1000.0f * result - 500.0f) / 10.0f;
    return temperature;
}

// Converts a raw ADC count to voltage (V) using the 1.8V reference and 12-bit range
float calcVoltage(uint32_t voltage)
{
    return A2D_VOLTAGE_REF_V * ((float)voltage / (float)A2D_MAX_READING);
}

// Generic sensor loop
typedef struct {
    const char    *adc_path;        // sysfs IIO path for this sensor
    uint32_t       min_raw;         // ADC lower bound
    uint32_t       max_raw;         // ADC upper bound
    sensor_idx_t   sensor_idx;      // index into thread_data.value and g_mutex_sensor
    float         *out;             // pointer to output field in thread_data
    int            watchdog_idx;
    long long      poll_ms;
} sensor_cfg_t;

// Polls one sensor at cfg->poll_ms, applies EMA, and writes the result; kicks watchdog only on a valid read
static void runSensorLoop(struct thread_data *data, const sensor_cfg_t *cfg)
{
    bool initialized = false;
    float ema = 0.0f;

    pthread_mutex_lock(&g_mutex_sensor[cfg->sensor_idx]);
    {
        *cfg->out = 0.0f;
    }
    pthread_mutex_unlock(&g_mutex_sensor[cfg->sensor_idx]);

    while (1) {
        bool end_thread = false;
        pthread_mutex_lock(&g_mutex_control);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&g_mutex_control);

        if (end_thread) return;

        uint32_t raw = 0;
        if (getVoltageReading(cfg->adc_path, &raw) && sensorReadValid(raw, cfg->min_raw, cfg->max_raw)) {
            float sample = (cfg->sensor_idx == SENSOR_TEMP) ? calcVoltageTemp(raw) : calcVoltage(raw);
            ema = initialized ? EMA_ALPHA * sample + (1.0f - EMA_ALPHA) * ema : sample;
            initialized = true;

            pthread_mutex_lock(&g_mutex_sensor[cfg->sensor_idx]);
            {
                *cfg->out = ema;
            }
            pthread_mutex_unlock(&g_mutex_sensor[cfg->sensor_idx]);

            watchdogKick(data, cfg->watchdog_idx);
        }

        sleepForMs(cfg->poll_ms);
    }
}

// Thread entry points
// These functions are all wrappers around runSensorLoop
void *readTemperature(void *arg)
{
    struct thread_data *data = arg;
    sensor_cfg_t cfg = {
        .adc_path     = A2D_FILE_VOLTAGE0,
        .min_raw      = SENSOR_MIN_TEMP,
        .max_raw      = SENSOR_MAX_TEMP,
        .sensor_idx   = SENSOR_TEMP,
        .out          = &data->value[SENSOR_TEMP],
        .watchdog_idx = WATCHDOG_IDX_TEMP,
        .poll_ms      = 500,
    };
    runSensorLoop(data, &cfg);
    return NULL;
}

void *readIR(void *arg)
{
    struct thread_data *data = arg;
    sensor_cfg_t cfg = {
        .adc_path     = A2D_FILE_VOLTAGE1,
        .min_raw      = SENSOR_MIN_IR,
        .max_raw      = SENSOR_MAX_IR,
        .sensor_idx= SENSOR_IR,
        .out          = &data->ema_ir,   // calculateStatus reads ema_ir and publishes to value[SENSOR_IR]
        .watchdog_idx = WATCHDOG_IDX_IR,
        .poll_ms      = 50,
    };
    runSensorLoop(data, &cfg);
    return NULL;
}

void *readCO(void *arg)
{
    struct thread_data *data = arg;
    sensor_cfg_t cfg = {
        .adc_path     = A2D_FILE_VOLTAGE5,
        .min_raw      = SENSOR_MIN_CO,
        .max_raw      = SENSOR_MAX_CO,
        .sensor_idx= SENSOR_CO,
        .out          = &data->value[SENSOR_CO],
        .watchdog_idx = WATCHDOG_IDX_CO,
        .poll_ms      = 500,
    };
    runSensorLoop(data, &cfg);
    return NULL;
}

void *readCO2(void *arg)
{
    struct thread_data *data = arg;
    sensor_cfg_t cfg = {
        .adc_path     = A2D_FILE_VOLTAGE2,
        .min_raw      = SENSOR_MIN_CO2,
        .max_raw      = SENSOR_MAX_CO2,
        .sensor_idx= SENSOR_CO2,
        .out          = &data->value[SENSOR_CO2],
        .watchdog_idx = WATCHDOG_IDX_CO2,
        .poll_ms      = 500,
    };
    runSensorLoop(data, &cfg);
    return NULL;
}

void *readSmoke(void *arg)
{
    struct thread_data *data = arg;
    sensor_cfg_t cfg = {
        .adc_path     = A2D_FILE_VOLTAGE3,
        .min_raw      = SENSOR_MIN_SMOKE,
        .max_raw      = SENSOR_MAX_SMOKE,
        .sensor_idx= SENSOR_SMOKE,
        .out          = &data->value[SENSOR_SMOKE],
        .watchdog_idx = WATCHDOG_IDX_SMOKE,
        .poll_ms      = 1000,
    };
    runSensorLoop(data, &cfg);
    return NULL;
}
