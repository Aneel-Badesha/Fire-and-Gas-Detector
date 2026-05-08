// Header file common functions/libraries between modules

#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <math.h>
#include <fcntl.h>
#include <string.h>

#define IRPOINT 1.75
#define TEMPPOINT 27.0
#define COPOINT 0.9
#define CO2POINT 0.9
#define SMOKEPOINT 0.65

#define BUFFER_SIZE 10

#define WATCHDOG_TIMEOUT_S 5
#define WATCHDOG_THREAD_COUNT 6

#define WATCHDOG_IDX_TEMP   0
#define WATCHDOG_IDX_IR     1
#define WATCHDOG_IDX_AIR    2
#define WATCHDOG_IDX_STATUS 3
#define WATCHDOG_IDX_ALARM  4
#define WATCHDOG_IDX_OUTPUT 5

// Mutex acquisition order to avoid deadlock, threads MUST acquire these
// locks in this order (and release in reverse)
//
//   1. mutexControl
//   2. mutexAir
//   3. mutexTemp
//   4. mutexIR
//   5. mutexAlarm
//   6. mutexWatchdog
//
// Most threads only need one lock at a time
struct thread_data
{
    pthread_mutex_t mutexControl, mutexTemp, mutexIR, mutexAir, mutexAlarm;
    pthread_mutex_t mutexWatchdog;
    pthread_t id_user, id_temp, id_air, id_IR, id_status, id_output, id_alarm, id_watchdog;

    // Uses mutexControl
    bool end_all_threads;

    // Uses mutexWatchdog
    time_t watchdog_kicks[WATCHDOG_THREAD_COUNT];

    double temp_value, smoke_value, CO_value, CO2_value, IR_value; 
    bool alarm_temp, alarm_smoke, alarm_CO, alarm_CO2, alarm_IR;
    bool warning_alarm, general_alarm, obstructed_alarm;

    double IR_buffer[BUFFER_SIZE];
};

// Sleep in ms
void sleepForMs(long long delayInMs);

// Kick the watchdog timer for the given thread index
void watchdogKick(struct thread_data *data, int thread_idx);

#endif
