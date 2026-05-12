// Project main file

#include "common.h"
#include "sensor.h"
#include "output.h"
#include "user.h"
#include "watchdog.h"

// Initialises hardware pins, spawns all sensor/logic/watchdog threads, and waits for shutdown
int main(void)
{
    if (system("config-pin P9_17 i2c") != 0) {
        fprintf(stderr, "Error: Failed to configure pin P9_17 for I2C\n");
    }
    if (system("config-pin P9_18 i2c") != 0) {
        fprintf(stderr, "Error: Failed to configure pin P9_18 for I2C\n");
    }

    globalsInit();

    struct thread_data td;
    td.end_all_threads  = false;
    td.general_alarm    = false;
    td.obstructed_alarm = false;

    time_t now = time(NULL);
    for (int i = 0; i < WATCHDOG_THREAD_IDX_MAX; i++) {
        td.watchdog_kicks[i] = now;
    }
    for (int i = 0; i < SENSOR_MAX; i++) {
        td.value[i] = 0.0f;
    }

    if (pthread_create(&g_tid[THREAD_IDX_USER], NULL, exitProgram, &td) != 0) {
        fprintf(stderr, "Error creating user thread\n");
        return 1;
    }
    if (pthread_create(&g_tid[THREAD_IDX_TEMP], NULL, readTemperature, &td) != 0) {
        fprintf(stderr, "Error creating temperature thread\n");
        return 1;
    }
    if (pthread_create(&g_tid[THREAD_IDX_IR], NULL, readIR, &td) != 0) {
        fprintf(stderr, "Error creating IR thread\n");
        return 1;
    }
    if (pthread_create(&g_tid[THREAD_IDX_CO], NULL, readCO, &td) != 0) {
        fprintf(stderr, "Error creating CO thread\n");
        return 1;
    }
    if (pthread_create(&g_tid[THREAD_IDX_CO2], NULL, readCO2, &td) != 0) {
        fprintf(stderr, "Error creating CO2 thread\n");
        return 1;
    }
    if (pthread_create(&g_tid[THREAD_IDX_SMOKE], NULL, readSmoke, &td) != 0) {
        fprintf(stderr, "Error creating smoke thread\n");
        return 1;
    }

    sleepForMs(2000);

    if (pthread_create(&g_tid[THREAD_IDX_ALARM], NULL, calcAlarm, &td) != 0) {
        fprintf(stderr, "Error creating alarm thread\n");
        return 1;
    }
    if (pthread_create(&g_tid[THREAD_IDX_OUTPUT], NULL, displayOutput, &td) != 0) {
        fprintf(stderr, "Error creating output thread\n");
        return 1;
    }
    if (pthread_create(&g_tid[THREAD_IDX_WATCHDOG], NULL, watchdogMonitor, &td) != 0) {
        fprintf(stderr, "Error creating watchdog thread\n");
        return 1;
    }

    while (1) {
        pthread_mutex_lock(&g_mutex_control);
        {
            if (td.end_all_threads == true) {
                pthread_mutex_unlock(&g_mutex_control);
                pthread_join(g_tid[THREAD_IDX_USER],     NULL);
                pthread_join(g_tid[THREAD_IDX_WATCHDOG], NULL);
                break;
            }
        }
        pthread_mutex_unlock(&g_mutex_control);

        sleepForMs(1000);
    }

    printf("Exiting Program!\n");
    globalsDestroy();
    return 0;
}
