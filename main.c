// Project main file

#include "common.h"
#include "sensor.h"
#include "output.h"
#include "user.h"
#include "watchdog.h"

int main(void)
{
    if (system("config-pin P9_17 i2c") != 0) {
        fprintf(stderr, "Error: Failed to configure pin P9_17 for I2C\n");
    }
    if (system("config-pin P9_18 i2c") != 0) {
        fprintf(stderr, "Error: Failed to configure pin P9_18 for I2C\n");
    }

    struct thread_data thread_data;
    thread_data.end_all_threads = false;

    // Initialize watchdog kick times to now so threads have time to start
    time_t now = time(NULL);
    for (int i = 0; i < WATCHDOG_THREAD_COUNT; i++) {
        thread_data.watchdog_kicks[i] = now;
    }

    // Initialize mutexes
    pthread_mutex_init(&thread_data.mutexControl, NULL);
    pthread_mutex_init(&thread_data.mutexTemp, NULL);
    pthread_mutex_init(&thread_data.mutexIR, NULL);
    pthread_mutex_init(&thread_data.mutexAir, NULL);
    pthread_mutex_init(&thread_data.mutexAlarm, NULL);
    pthread_mutex_init(&thread_data.mutexWatchdog, NULL);

    // Start threads
    if (pthread_create(&thread_data.id_user, NULL, exitProgram, &thread_data) != 0) {
        fprintf(stderr, "Error creating user thread\n");
        return 1;
    }
    if (pthread_create(&thread_data.id_temp, NULL, readTemperature, &thread_data) != 0) {
        fprintf(stderr, "Error creating temperature thread\n");
        return 1;
    }
    if (pthread_create(&thread_data.id_IR, NULL, readIR, &thread_data) != 0) {
        fprintf(stderr, "Error creating IR thread\n");
        return 1;
    }
    if (pthread_create(&thread_data.id_air, NULL, readAirSensors, &thread_data) != 0) {
        fprintf(stderr, "Error creating air sensors thread\n");
        return 1;
    }
    

    sleepForMs(1000);
    if (pthread_create(&thread_data.id_status, NULL, calculateStatus, &thread_data) != 0) {
        fprintf(stderr, "Error creating status thread\n");
        return 1;
    }
    if (pthread_create(&thread_data.id_alarm, NULL, calcAlarm, &thread_data) != 0) {
        fprintf(stderr, "Error creating alarm thread\n");
        return 1;
    }
    if (pthread_create(&thread_data.id_output, NULL, displayOutput, &thread_data) != 0) {
        fprintf(stderr, "Error creating output thread\n");
        return 1;
    }
    if (pthread_create(&thread_data.id_watchdog, NULL, watchdogMonitor, &thread_data) != 0) {
        fprintf(stderr, "Error creating watchdog thread\n");
        return 1;
    }

    while(1) {
        
        // Check for USER button
        pthread_mutex_lock(&thread_data.mutexControl);
        {
            if(thread_data.end_all_threads == true) {
                // Make sure to unlock mutex before waiting
                pthread_mutex_unlock(&thread_data.mutexControl);
                pthread_join(thread_data.id_user, NULL);
                pthread_join(thread_data.id_watchdog, NULL);
                break;
            }
        }
        pthread_mutex_unlock(&thread_data.mutexControl);  
 
        sleepForMs(1000);
    }

    printf("Exiting Program!\n");

    // Destroy mutexes
    pthread_mutex_destroy(&thread_data.mutexControl);
    pthread_mutex_destroy(&thread_data.mutexTemp);
    pthread_mutex_destroy(&thread_data.mutexIR);
    pthread_mutex_destroy(&thread_data.mutexAir);
    pthread_mutex_destroy(&thread_data.mutexAlarm);
    pthread_mutex_destroy(&thread_data.mutexWatchdog);

    return 0;
}
