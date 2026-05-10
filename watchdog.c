#include "watchdog.h"

static const char *const thread_names[THREAD_COUNT] = {
    "user", "temperature", "IR", "CO", "CO2", "smoke", "status", "alarm", "output", "watchdog"
};

// Checks per-thread heartbeat timestamps every 1s and triggers shutdown if any thread stops kicking
void *watchdogMonitor(void *arg)
{
    struct thread_data *data = arg;
    bool end_thread = false;

    // Give threads time to start and register their first kick
    sleepForMs(1000);

    while (1) {
        pthread_mutex_lock(&g_mutex_control);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&g_mutex_control);

        if (end_thread) {
            return NULL;
        }

        time_t now = time(NULL);
        int failed_idx = -1;

        pthread_mutex_lock(&g_mutex_watchdog);
        {
            for (int i = 0; i < WATCHDOG_THREAD_COUNT; i++) {
                if (now - data->watchdog_kicks[i] > WATCHDOG_TIMEOUT_S) {
                    failed_idx = i;
                    break;
                }
            }
        }
        pthread_mutex_unlock(&g_mutex_watchdog);

        if (failed_idx >= 0) {
            printf("WATCHDOG: %s thread stopped responding, triggering shutdown...\n",
                   thread_names[failed_idx]);
            pthread_mutex_lock(&g_mutex_control);
            {
                data->end_all_threads = true;
            }
            pthread_mutex_unlock(&g_mutex_control);
            return NULL;
        }

        sleepForMs(1000);
    }
}
