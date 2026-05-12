// Shared types, constants, and utility function declarations used across all modules

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

// Alarm threshold voltages
#define IRPOINT    1.75f
#define COPOINT    0.9f
#define CO2POINT   0.9f
#define SMOKEPOINT 0.65f
#define TEMPPOINT  27.0f

#define EMA_ALPHA 0.6f              // EMA alpha value, means new value has 60% influence vs 40% memory
#define WATCHDOG_TIMEOUT_S 5        // watchdog timeout in seconds for all threads

// Sensor indices for thread_data.value, thread_data.alarm, g_mutex_sensor
typedef enum {
    SENSOR_TEMP = 0,
    SENSOR_IR,
    SENSOR_CO,
    SENSOR_CO2,
    SENSOR_SMOKE,
    SENSOR_MAX
} sensor_idx_t;

// Thread indices for g_tid
typedef enum {
    THREAD_IDX_USER = 0,
    THREAD_IDX_TEMP,
    THREAD_IDX_IR,
    THREAD_IDX_CO,
    THREAD_IDX_CO2,
    THREAD_IDX_SMOKE,
    THREAD_IDX_ALARM,
    THREAD_IDX_OUTPUT,
    THREAD_IDX_WATCHDOG,
    THREAD_IDX_MAX
} thread_idx_t;

// Watchdog indices index into thread_data.watchdog_kicks
// Must stay in sync with THREAD_IDX_MAX
#define WATCHDOG_THREAD_IDX_MAX THREAD_IDX_MAX
#define WATCHDOG_IDX_USER    THREAD_IDX_USER
#define WATCHDOG_IDX_TEMP    THREAD_IDX_TEMP
#define WATCHDOG_IDX_IR      THREAD_IDX_IR
#define WATCHDOG_IDX_CO      THREAD_IDX_CO
#define WATCHDOG_IDX_CO2     THREAD_IDX_CO2
#define WATCHDOG_IDX_SMOKE   THREAD_IDX_SMOKE
#define WATCHDOG_IDX_ALARM   THREAD_IDX_ALARM
#define WATCHDOG_IDX_OUTPUT  THREAD_IDX_OUTPUT
#define WATCHDOG_IDX_WATCHDOG THREAD_IDX_WATCHDOG

// Global mutexes and thread IDs
// Defined in common.c, declared here
extern pthread_mutex_t g_mutex_control;             // protects end_all_threads
extern pthread_mutex_t g_mutex_sensor[SENSOR_MAX];  // protects value[], one per sensor
extern pthread_mutex_t g_mutex_alarm;               // protects general_alarm, obstructed_alarm
extern pthread_mutex_t g_mutex_watchdog;            // protects watchdog_kicks
extern pthread_t       g_tid[THREAD_IDX_MAX];       // thread IDs indexed by thread_idx_t, used to join on shutdown

// Shared runtime state passed to every thread
struct thread_data {
    bool end_all_threads;                               // signal for all threads to exit cleanly
    time_t watchdog_kicks[WATCHDOG_THREAD_IDX_MAX];     // last kick timestamp per thread, checked by watchdog
    float value[SENSOR_MAX];                            // latest sensor output, indexed by sensor_idx_t
    bool general_alarm;                                 // true if any sensor alarm is active
    bool obstructed_alarm;                              // true if IR sensor detects obstruction
};

// Initialise/destroy all global mutexes
void globalsInit(void);
void globalsDestroy(void);

void sleepForMs(long long delay_ms);
void watchdogKick(struct thread_data *data, int thread_idx);

#endif
