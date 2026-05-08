#include "common.h"

void watchdogKick(struct thread_data *data, int thread_idx)
{
    pthread_mutex_lock(&data->mutexWatchdog);
    {
        data->watchdog_kicks[thread_idx] = time(NULL);
    }
    pthread_mutex_unlock(&data->mutexWatchdog);
}

void sleepForMs(long long delayInMs)
{
    const long long NS_PER_MS = 1000 * 1000;
    const long long NS_PER_SECOND = 1000000000;
    long long delayNs = delayInMs * NS_PER_MS;
    int seconds = delayNs / NS_PER_SECOND;
    int nanoseconds = delayNs % NS_PER_SECOND;
    struct timespec reqDelay = {seconds, nanoseconds};
    nanosleep(&reqDelay, (struct timespec *) NULL);
} 
