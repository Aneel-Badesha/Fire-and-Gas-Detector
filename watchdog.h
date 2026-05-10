// Watchdog monitor that shuts down the system if any thread stops responding

#ifndef WATCHDOG_H_
#define WATCHDOG_H_

#include "common.h"

// Checks all worker thread heartbeats every second and triggers shutdown on timeout
void *watchdogMonitor(void *arg);

#endif
