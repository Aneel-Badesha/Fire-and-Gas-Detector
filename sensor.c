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

double calVoltageTemp(int voltage) // compute the int voltage reading to double (from 0)
{
    double result = (A2D_VOLTAGE_REF_V) * ((double)voltage / (double)A2D_MAX_READING);
    double temperature = (double)(1000 * result - 500) / (double)10;
    return temperature;
}

double calVoltage(int voltage)  {

    double value = (A2D_VOLTAGE_REF_V) * ((double)voltage / (double)A2D_MAX_READING);
    return value;
}

void *readTemperature(void *arg)
{
    struct thread_data *data = arg;
    bool end_thread = false;
    bool initialization = true;
    int index = 0;

    int raw_data = 0;
    double cal_values[BUFFER_SIZE] = {0};

    pthread_mutex_lock(&data->mutexTemp);
    {
        data->temp_value = 0;
    }
    pthread_mutex_unlock(&data->mutexTemp);

    while(1) {

        pthread_mutex_lock(&data->mutexControl);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&data->mutexControl);

        watchdogKick(data, WATCHDOG_IDX_TEMP);

        if(end_thread == false) {
            index = index % BUFFER_SIZE;

            if(initialization == true) {
                for(int i = 0; i < BUFFER_SIZE; i++) {
                    // First initialization
                    if (getVoltageReading(A2D_FILE_VOLTAGE0, &raw_data)) {
                        cal_values[i] = calVoltageTemp(raw_data);
                    }
                    // Fill up buffer quickly
                    sleepForMs(10);
                }

                initialization = false;
            }
            else {
                if (getVoltageReading(A2D_FILE_VOLTAGE0, &raw_data)) {
                    cal_values[index] = calVoltageTemp(raw_data);
                }
                index++;
            }
            
            double value = 0;
            for(int i = 0; i < BUFFER_SIZE; i++) {
                // Calculate average
                value = value + cal_values[i];
            }

            pthread_mutex_lock(&data->mutexTemp);
            {
                data->temp_value = value / BUFFER_SIZE;
            }
            pthread_mutex_unlock(&data->mutexTemp);
     
            sleepForMs(250);
            
        } 
        else {
            return NULL;
        }
    }
}

void *readIR(void *arg)
{
    
    struct thread_data *data = arg;
    bool end_thread = false;
    bool initialization = true;

    int index = 0;
    int raw_data = 0;

    pthread_mutex_lock(&data->mutexIR);
    {
        data->IR_value = 0;
        for (int i = 0; i < BUFFER_SIZE; i++) {
            data->IR_buffer[i] = 0.0;
        }
    }
    pthread_mutex_unlock(&data->mutexIR);

    while(1) {
        pthread_mutex_lock(&data->mutexControl);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&data->mutexControl);

        watchdogKick(data, WATCHDOG_IDX_IR);

        if(end_thread == false) {
            index = index % BUFFER_SIZE;

            if(initialization == true) {
                for(int i = 0; i < BUFFER_SIZE; i++) {
                    if (getVoltageReading(A2D_FILE_VOLTAGE1, &raw_data)) {
                        pthread_mutex_lock(&data->mutexIR);
                        {
                            data->IR_buffer[i] = calVoltage(raw_data);
                        }
                        pthread_mutex_unlock(&data->mutexIR);
                    }

                    sleepForMs(10);
                }

                initialization = false;
            }
            else {
                if (getVoltageReading(A2D_FILE_VOLTAGE1, &raw_data)) {
                    pthread_mutex_lock(&data->mutexIR);
                    {
                        data->IR_buffer[index] = calVoltage(raw_data);
                    }
                    pthread_mutex_unlock(&data->mutexIR);
                }
                index++;
            }

            sleepForMs(250);
        } 
        else {
            return NULL;
        }
    }
}

void *readAirSensors(void *arg)
{
    struct thread_data *data = arg;
    bool end_thread = false;

    pthread_mutex_lock(&data->mutexAir);
    {
        data->CO_value = 0.0;
        data->CO2_value = 0.0;
        data->smoke_value = 0.0;
    }
    pthread_mutex_unlock(&data->mutexAir);

    while(1) {
        pthread_mutex_lock(&data->mutexControl);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&data->mutexControl);

        watchdogKick(data, WATCHDOG_IDX_AIR);

        if(end_thread == false) {
            int raw_CO = 0, raw_CO2 = 0, raw_smoke = 0;
            bool ok_CO    = getVoltageReading(A2D_FILE_VOLTAGE5, &raw_CO);
            bool ok_CO2   = getVoltageReading(A2D_FILE_VOLTAGE2, &raw_CO2);
            bool ok_smoke = getVoltageReading(A2D_FILE_VOLTAGE3, &raw_smoke);

            pthread_mutex_lock(&data->mutexAir);
            {
                if (ok_CO)    data->CO_value    = calVoltage(raw_CO);
                if (ok_CO2)   data->CO2_value   = calVoltage(raw_CO2);
                if (ok_smoke) data->smoke_value = calVoltage(raw_smoke);
            }
            pthread_mutex_unlock(&data->mutexAir);

            sleepForMs(250);
        }
        else {
            return NULL;
        }
    }
}

