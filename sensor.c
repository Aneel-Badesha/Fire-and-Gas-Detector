#include <inttypes.h>
#include "sensor.h"

// Reads a raw 12-bit ADC count from the IIO path, returns false on failure
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

// Converts a raw ADC count to voltage using the 1.8V vref and 12-bit range
static float calcVoltage(uint32_t voltage)
{
    return A2D_VOLTAGE_REF_V * ((float)voltage / (float)A2D_MAX_READING);
}

// Converts a raw ADC count to Celsius using the TMP36 transfer function
static float calcTemp(uint32_t voltage)
{
    float result = calcVoltage(voltage);
    float temperature = ((1000.0f * result) - 500.0f) / 10.0f;
    return temperature;
}

// Polls one sensor at poll_ms, applies EMA, and writes the result
// Kicks watchdog only on a valid read
static void runSensorLoop(struct thread_data *data, const sensor_cfg_t *cfg)
{
    bool initialized = false;
    float ema = 0.0f;

    pthread_mutex_lock(&g_mutex_sensor[cfg->sensor_idx]);
    {
        *cfg->ema_out = 0.0f;
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
            float sample = (cfg->sensor_idx == SENSOR_TEMP) ? calcTemp(raw) : calcVoltage(raw);
            ema = initialized ? EMA_ALPHA * sample + (1.0f - EMA_ALPHA) * ema : sample;
            initialized = true;

            pthread_mutex_lock(&g_mutex_sensor[cfg->sensor_idx]);
            {
                *cfg->ema_out = ema;
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
        .ema_out      = &data->value[SENSOR_TEMP],
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
        .ema_out      = &data->value[SENSOR_IR],
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
        .ema_out      = &data->value[SENSOR_CO],
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
        .ema_out      = &data->value[SENSOR_CO2],
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
        .ema_out      = &data->value[SENSOR_SMOKE],
        .watchdog_idx = WATCHDOG_IDX_SMOKE,
        .poll_ms      = 1000,
    };
    runSensorLoop(data, &cfg);
    return NULL;
}
