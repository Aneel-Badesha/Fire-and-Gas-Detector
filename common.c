#include "common.h"

// Global mutex and thread ID storage (declared extern in common.h)
pthread_mutex_t g_mutex_control;
pthread_mutex_t g_mutex_sensor[SENSOR_MAX];
pthread_mutex_t g_mutex_alarm;
pthread_mutex_t g_mutex_watchdog;
pthread_t       g_tid[THREAD_IDX_MAX];

// Initialises all global mutexes
void globalsInit(void)
{
    pthread_mutex_init(&g_mutex_control,  NULL);
    pthread_mutex_init(&g_mutex_alarm,    NULL);
    pthread_mutex_init(&g_mutex_watchdog, NULL);
    for (int i = 0; i < SENSOR_MAX; i++) {
        pthread_mutex_init(&g_mutex_sensor[i], NULL);
    }
}

// Destroys all global mutexes
void globalsDestroy(void)
{
    pthread_mutex_destroy(&g_mutex_control);
    pthread_mutex_destroy(&g_mutex_alarm);
    pthread_mutex_destroy(&g_mutex_watchdog);
    for (int i = 0; i < SENSOR_MAX; i++) {
        pthread_mutex_destroy(&g_mutex_sensor[i]);
    }
}

// Updates the watchdog heartbeat timestamp for the given thread index
void watchdogKick(struct thread_data *data, int thread_idx)
{
    pthread_mutex_lock(&g_mutex_watchdog);
    {
        data->watchdog_kicks[thread_idx] = time(NULL);
    }
    pthread_mutex_unlock(&g_mutex_watchdog);
}

// Sleeps the calling thread for the specified number of milliseconds
void sleepForMs(long long delay_ms)
{
    const long long NS_PER_MS     = 1000 * 1000;
    const long long NS_PER_SECOND = 1000000000;
    long long delay_ns  = delay_ms * NS_PER_MS;
    int seconds         = delay_ns / NS_PER_SECOND;
    int nanoseconds     = delay_ns % NS_PER_SECOND;
    struct timespec req_delay = {seconds, nanoseconds};
    nanosleep(&req_delay, (struct timespec *)NULL);
}
