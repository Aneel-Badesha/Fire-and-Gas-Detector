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

// Alarm threshold voltages (V)
#define IRPOINT    1.75f
#define COPOINT    0.9f
#define CO2POINT   0.9f
#define SMOKEPOINT 0.65f
#define TEMPPOINT  27.0f

#define EMA_ALPHA          0.6f
#define WATCHDOG_TIMEOUT_S 5     // watchdog timeout (s) for all threads

// Sensor indices — index into thread_data.value[], thread_data.alarm[], g_mutex_sensor[]
typedef enum {
    SENSOR_TEMP = 0,
    SENSOR_IR,
    SENSOR_CO,
    SENSOR_CO2,
    SENSOR_SMOKE,
    SENSOR_COUNT
} sensor_idx_t;

// Thread indices — index into g_tid[]
typedef enum {
    THREAD_IDX_USER = 0,
    THREAD_IDX_TEMP,
    THREAD_IDX_IR,
    THREAD_IDX_CO,
    THREAD_IDX_CO2,
    THREAD_IDX_SMOKE,
    THREAD_IDX_STATUS,
    THREAD_IDX_ALARM,
    THREAD_IDX_OUTPUT,
    THREAD_IDX_WATCHDOG,
    THREAD_COUNT
} thread_idx_t;

// Watchdog indices index into thread_data.watchdog_kicks
// Must stay in sync with THREAD_COUNT (one entry per monitored thread)
#define WATCHDOG_THREAD_COUNT THREAD_COUNT
#define WATCHDOG_IDX_USER    THREAD_IDX_USER
#define WATCHDOG_IDX_TEMP    THREAD_IDX_TEMP
#define WATCHDOG_IDX_IR      THREAD_IDX_IR
#define WATCHDOG_IDX_CO      THREAD_IDX_CO
#define WATCHDOG_IDX_CO2     THREAD_IDX_CO2
#define WATCHDOG_IDX_SMOKE   THREAD_IDX_SMOKE
#define WATCHDOG_IDX_STATUS  THREAD_IDX_STATUS
#define WATCHDOG_IDX_ALARM   THREAD_IDX_ALARM
#define WATCHDOG_IDX_OUTPUT  THREAD_IDX_OUTPUT
#define WATCHDOG_IDX_WATCHDOG THREAD_IDX_WATCHDOG

// Global mutexes and thread IDs
// Defined in common.c, declared here
extern pthread_mutex_t g_mutex_control;
extern pthread_mutex_t g_mutex_sensor[SENSOR_COUNT]; // one per sensor, indexed by sensor_idx_t
extern pthread_mutex_t g_mutex_alarm;
extern pthread_mutex_t g_mutex_watchdog;
extern pthread_t       g_tid[THREAD_COUNT];

// Initialise / destroy all global mutexes
void globalsInit(void);
void globalsDestroy(void);

// Shared runtime state passed to every thread
struct thread_data {
    // Protected by g_mutex_control
    bool end_all_threads;

    // Protected by g_mutex_watchdog
    time_t watchdog_kicks[WATCHDOG_THREAD_COUNT];

    // Protected by g_mutex_sensor[i]
    float value[SENSOR_COUNT];
    float ema_ir; // written by readIR at 50ms; calculateStatus reads and publishes to value[SENSOR_IR]

    // Protected by g_mutex_alarm
    bool alarm[SENSOR_COUNT];
    bool general_alarm;
    bool obstructed_alarm;
};

void sleepForMs(long long delay_ms);
void watchdogKick(struct thread_data *data, int thread_idx);

#endif
