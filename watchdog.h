// Watchdog monitor that shuts down the system if any thread stops responding

#ifndef WATCHDOG_H_
#define WATCHDOG_H_

#include "common.h"

#define HW_WATCHDOG_TIMEOUT_S 15
#define HW_WATCHDOG_DEVICE    "/dev/watchdog"

// Checks all worker thread heartbeats every second and triggers shutdown on timeout
void *watchdogMonitor(void *arg);

#endif
