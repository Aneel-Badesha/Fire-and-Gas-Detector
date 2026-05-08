#include "sensor.h"

// 0 = Temperature
// 1 = IR
// 2 = CO2
// 3 = Smoke / Air quality
// 5 = CO

bool getVoltageReading(const char *path, int *out_value)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "WARN: unable to open %s; reusing previous reading\n", path);
        return false;
    }

    int a2dReading = 0;
    int itemsRead = fscanf(f, "%d", &a2dReading);
    fclose(f);

    if (itemsRead <= 0) {
        fprintf(stderr, "WARN: unable to read from %s; reusing previous reading\n", path);
        return false;
    }

    *out_value = a2dReading;
    return true;
}

double calVoltageTemp(int voltage)
{
    double result = (A2D_VOLTAGE_REF_V) * ((double)voltage / (double)A2D_MAX_READING);
    double temperature = (double)(1000 * result - 500) / (double)10;
    return temperature;
}

double calVoltage(int voltage)
{
    double value = (A2D_VOLTAGE_REF_V) * ((double)voltage / (double)A2D_MAX_READING);
    return value;
}

void *readTemperature(void *arg)
{
    struct thread_data *data = arg;
    bool end_thread = false;
    bool initialized = false;
    double ema = 0.0;
    int raw_data = 0;

    pthread_mutex_lock(&data->mutexTemp);
    {
        data->temp_value = 0;
    }
    pthread_mutex_unlock(&data->mutexTemp);

    while (1) {
        pthread_mutex_lock(&data->mutexControl);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&data->mutexControl);

        watchdogKick(data, WATCHDOG_IDX_TEMP);

        if (end_thread == false) {
            if (getVoltageReading(A2D_FILE_VOLTAGE0, &raw_data)) {
                double sample = calVoltageTemp(raw_data);
                if (!initialized) {
                    ema = sample;
                    initialized = true;
                } else {
                    ema = EMA_ALPHA * sample + (1.0 - EMA_ALPHA) * ema;
                }
            }

            pthread_mutex_lock(&data->mutexTemp);
            {
                data->temp_value = ema;
            }
            pthread_mutex_unlock(&data->mutexTemp);

            sleepForMs(250);
        } else {
            return NULL;
        }
    }
}

void *readIR(void *arg)
{
    struct thread_data *data = arg;
    bool end_thread = false;
    bool initialized = false;
    double ema = 0.0;
    int raw_data = 0;

    pthread_mutex_lock(&data->mutexIR);
    {
        data->IR_value = 0;
        data->IR_ema = 0.0;
    }
    pthread_mutex_unlock(&data->mutexIR);

    while (1) {
        pthread_mutex_lock(&data->mutexControl);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&data->mutexControl);

        watchdogKick(data, WATCHDOG_IDX_IR);

        if (end_thread == false) {
            if (getVoltageReading(A2D_FILE_VOLTAGE1, &raw_data)) {
                double sample = calVoltage(raw_data);
                if (!initialized) {
                    ema = sample;
                    initialized = true;
                } else {
                    ema = EMA_ALPHA * sample + (1.0 - EMA_ALPHA) * ema;
                }

                pthread_mutex_lock(&data->mutexIR);
                {
                    data->IR_ema = ema;
                }
                pthread_mutex_unlock(&data->mutexIR);
            }

            sleepForMs(250);
        } else {
            return NULL;
        }
    }
}

void *readAirSensors(void *arg)
{
    struct thread_data *data = arg;
    bool end_thread = false;
    bool co_init = false, co2_init = false, smoke_init = false;
    double co_ema = 0.0, co2_ema = 0.0, smoke_ema = 0.0;

    pthread_mutex_lock(&data->mutexAir);
    {
        data->CO_value = 0.0;
        data->CO2_value = 0.0;
        data->smoke_value = 0.0;
    }
    pthread_mutex_unlock(&data->mutexAir);

    while (1) {
        pthread_mutex_lock(&data->mutexControl);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&data->mutexControl);

        watchdogKick(data, WATCHDOG_IDX_AIR);

        if (end_thread == false) {
            int raw_CO = 0, raw_CO2 = 0, raw_smoke = 0;
            bool ok_CO    = getVoltageReading(A2D_FILE_VOLTAGE5, &raw_CO);
            bool ok_CO2   = getVoltageReading(A2D_FILE_VOLTAGE2, &raw_CO2);
            bool ok_smoke = getVoltageReading(A2D_FILE_VOLTAGE3, &raw_smoke);

            if (ok_CO) {
                double s = calVoltage(raw_CO);
                co_ema = co_init ? EMA_ALPHA * s + (1.0 - EMA_ALPHA) * co_ema : s;
                co_init = true;
            }
            if (ok_CO2) {
                double s = calVoltage(raw_CO2);
                co2_ema = co2_init ? EMA_ALPHA * s + (1.0 - EMA_ALPHA) * co2_ema : s;
                co2_init = true;
            }
            if (ok_smoke) {
                double s = calVoltage(raw_smoke);
                smoke_ema = smoke_init ? EMA_ALPHA * s + (1.0 - EMA_ALPHA) * smoke_ema : s;
                smoke_init = true;
            }

            pthread_mutex_lock(&data->mutexAir);
            {
                if (ok_CO)    data->CO_value    = co_ema;
                if (ok_CO2)   data->CO2_value   = co2_ema;
                if (ok_smoke) data->smoke_value  = smoke_ema;
            }
            pthread_mutex_unlock(&data->mutexAir);

            sleepForMs(250);
        } else {
            return NULL;
        }
    }
}
