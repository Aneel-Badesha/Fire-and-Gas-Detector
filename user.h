// Header file for user button
#ifndef USER_H_
#define USER_H_

#include "common.h"
#define USERBUTTON "/sys/class/gpio/gpio72/value"

// Function to get value of USER button
int UserButtonValue();

// Detects when USER button is pressed and ends program
void *exitProgram(void *arg);

#endif
