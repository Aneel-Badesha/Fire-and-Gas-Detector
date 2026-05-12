#include "watchdog.h"

#include <errno.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>

static const char *const thread_names[THREAD_IDX_MAX] = {
    "user", "temperature", "IR", "CO", "CO2", "smoke", "alarm", "output", "watchdog"
};

static int hwWatchdogOpen(void)
{
    int fd = open(HW_WATCHDOG_DEVICE, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "WATCHDOG: cannot open %s: %s — running software-only\n",
                HW_WATCHDOG_DEVICE, strerror(errno));
        return -1;
    }

    struct watchdog_info info;
    if (ioctl(fd, WDIOC_GETSUPPORT, &info) == 0) {
        printf("WATCHDOG: hw device: %s\n", info.identity);
    }

    int timeout = HW_WATCHDOG_TIMEOUT_S;
    if (ioctl(fd, WDIOC_SETTIMEOUT, &timeout) < 0) {
        fprintf(stderr, "WATCHDOG: SETTIMEOUT failed: %s — using driver default\n",
                strerror(errno));
    } else {
        printf("WATCHDOG: hw timeout set to %ds\n", timeout);
    }

    return fd;
}

// Writing "V" before close tells the OMAP driver not to reset the board on fd close
static void hwWatchdogClose(int fd)
{
    if (fd < 0) return;
    if (write(fd, "V", 1) != 1) {
        fprintf(stderr, "WATCHDOG: magic close write failed: %s\n", strerror(errno));
    }
    close(fd);
}

// Checks per-thread heartbeat timestamps every 1s and triggers shutdown if any thread stops kicking
// Also pets the hardware watchdog each iteration, if this thread hangs, the board resets
void *watchdogMonitor(void *arg)
{
    struct thread_data *data = arg;
    bool end_thread = false;

    // Give threads time to start and register their first kick
    sleepForMs(1000);

    int hw_fd = hwWatchdogOpen();

    while (1) {
        pthread_mutex_lock(&g_mutex_control);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&g_mutex_control);

        if (end_thread) {
            hwWatchdogClose(hw_fd);
            return NULL;
        }

        time_t now = time(NULL);
        int failed_idx = -1;

        pthread_mutex_lock(&g_mutex_watchdog);
        {
            for (int i = 0; i < WATCHDOG_THREAD_IDX_MAX; i++) {
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
            hwWatchdogClose(hw_fd);
            return NULL;
        }

        if (hw_fd >= 0) {
            if (write(hw_fd, "1", 1) != 1) {
                fprintf(stderr, "WATCHDOG: pet failed: %s\n", strerror(errno));
            }
        }

        sleepForMs(1000);
    }
}
