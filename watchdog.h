// Header file for watchdog timer

#ifndef WATCHDOG_H_
#define WATCHDOG_H_

#include "common.h"

// Monitors all worker threads; triggers shutdown if any stop responding
void *watchdogMonitor(void *arg);

#endif
