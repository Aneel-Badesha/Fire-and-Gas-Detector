// User button detection and graceful program shutdown

#ifndef USER_H_
#define USER_H_

#include "common.h"

#define USERBUTTON "/sys/class/gpio/gpio72/value"  // sysfs path for the BeagleBone USER button

// Reads the USER button GPIO value, returns 0 when pressed, 1 when released
int userButtonValue(void);

// Polls USER button and signals all threads to exit when pressed
void *exitProgram(void *arg);

#endif
