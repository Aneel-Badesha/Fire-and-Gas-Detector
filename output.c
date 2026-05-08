#include "output.h"

#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

static const unsigned char LED_ROW_REGS[8] = {
    LED_REG_ROW0, LED_REG_ROW1, LED_REG_ROW2, LED_REG_ROW3,
    LED_REG_ROW4, LED_REG_ROW5, LED_REG_ROW6, LED_REG_ROW7,
};

// 8-row pattern: rendered as the "!" glyph used for the warning state.
static const unsigned char LED_PATTERN_WARNING[8] = {
    LED_PATTERN_EXCLAMATION, LED_PATTERN_EXCLAMATION, LED_PATTERN_EXCLAMATION,
    LED_PATTERN_EXCLAMATION, LED_PATTERN_EXCLAMATION, LED_PATTERN_OFF,
    LED_PATTERN_OFF,         LED_PATTERN_EXCLAMATION,
};

int i2cOpenBus(const char *bus_path, int slave_addr)
{
    int fd = open(bus_path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "WARN: cannot open %s: %s\n", bus_path, strerror(errno));
        return -1;
    }
    if (ioctl(fd, I2C_SLAVE, slave_addr) < 0) {
        fprintf(stderr, "WARN: I2C_SLAVE %#x failed: %s\n", slave_addr, strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

int i2cWriteCmd(int fd, unsigned char cmd)
{
    if (write(fd, &cmd, 1) != 1) {
        fprintf(stderr, "WARN: i2c cmd %#x failed: %s\n", cmd, strerror(errno));
        return -1;
    }
    return 0;
}

int i2cWriteReg(int fd, unsigned char reg, unsigned char value)
{
    unsigned char buf[2] = { reg, value };
    if (write(fd, buf, 2) != 2) {
        fprintf(stderr, "WARN: i2c reg %#x=%#x failed: %s\n", reg, value, strerror(errno));
        return -1;
    }
    return 0;
}

static void ledWritePattern(int fd, const unsigned char rows[8])
{
    if (fd < 0) return;
    for (int i = 0; i < 8; i++) {
        i2cWriteReg(fd, LED_ROW_REGS[i], rows[i]);
    }
}

static void ledFill(int fd, unsigned char value)
{
    if (fd < 0) return;
    for (int i = 0; i < 8; i++) {
        i2cWriteReg(fd, LED_ROW_REGS[i], value);
    }
}

bool thresholdHigh(double value, double tpoint) {
    return value > tpoint;
}

bool thresholdLow(double value, double tpoint) {
    return value < tpoint;
}

void *displayOutput(void *arg)
{
    struct thread_data *data = arg;
    bool end_thread = false;
    bool initialization = true;
    
    bool alarm_state = false;
    bool obstructed = false;

    while(1) {
        pthread_mutex_lock(&data->mutexControl);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&data->mutexControl);

        watchdogKick(data, WATCHDOG_IDX_OUTPUT);

        pthread_mutex_lock(&data->mutexAlarm);
        {
            alarm_state = data->general_alarm;
            obstructed = data->obstructed_alarm;
        }
        pthread_mutex_unlock(&data->mutexAlarm);

        if(end_thread == false) {
        if(initialization == true) {
            initialization = false;
            sleepForMs(1000);
            }
            
            else {
            if((obstructed == false) || (alarm_state == true)) {
                // Snapshot under each lock individually; print after release
                // so we never hold two locks at once.
                double co_v, co2_v, smoke_v, temp_v, ir_v;

                pthread_mutex_lock(&data->mutexAir);
                {
                    co_v    = data->CO_value;
                    co2_v   = data->CO2_value;
                    smoke_v = data->smoke_value;
                }
                pthread_mutex_unlock(&data->mutexAir);

                pthread_mutex_lock(&data->mutexTemp);
                {
                    temp_v = data->temp_value;
                }
                pthread_mutex_unlock(&data->mutexTemp);

                pthread_mutex_lock(&data->mutexIR);
                {
                    ir_v = data->IR_value;
                }
                pthread_mutex_unlock(&data->mutexIR);

                printf("\nTemperature: %.2f, IR Voltage: %.2f \n", temp_v, ir_v);
                printf("CO Voltage: %.2f, CO2 Voltage: %.2f, Smoke Voltage: %.2f \n\n",
                       co_v, co2_v, smoke_v);
            }
            else
            if((obstructed == true) && (alarm_state == false)) {
                printf("WARNING: Board likely obstructed!\n");
                printf("\n");
            }

            sleepForMs(2500);
        }
        }
        else 
        if(end_thread == true) {
            return NULL;
        }
        
        initialization = false;
        sleepForMs(2500);
    }

}

void *calculateStatus(void *arg)
{
    struct thread_data *data = arg;
    bool end_thread = false;
    bool initialization = true;

    while(1) {
        pthread_mutex_lock(&data->mutexControl);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&data->mutexControl);

        watchdogKick(data, WATCHDOG_IDX_STATUS);

        if(end_thread == false) {
        if(initialization == false) {

            // Snapshot raw sensor values under their own locks first, then
            // take mutexAlarm last (see lock order in common.h).
            double co_value, co2_value, smoke_value, temp_value;

            pthread_mutex_lock(&data->mutexAir);
            {
                co_value    = data->CO_value;
                co2_value   = data->CO2_value;
                smoke_value = data->smoke_value;
            }
            pthread_mutex_unlock(&data->mutexAir);

            pthread_mutex_lock(&data->mutexTemp);
            {
                temp_value = data->temp_value;
            }
            pthread_mutex_unlock(&data->mutexTemp);

            double ir_ema = 0.0;
            pthread_mutex_lock(&data->mutexIR);
            {
                ir_ema = data->IR_ema;
                data->IR_value = ir_ema;
            }
            pthread_mutex_unlock(&data->mutexIR);

            pthread_mutex_lock(&data->mutexAlarm);
            {
                data->alarm_IR    = thresholdHigh(ir_ema, IRPOINT);
                data->alarm_temp  = thresholdLow(temp_value, TEMPPOINT);
                data->alarm_CO    = thresholdHigh(co_value, COPOINT);
                data->alarm_CO2   = thresholdHigh(co2_value, CO2POINT);
                data->alarm_smoke = thresholdHigh(smoke_value, SMOKEPOINT);
            }
            pthread_mutex_unlock(&data->mutexAlarm);

            sleepForMs(500);
        }
        else {
            initialization = false;
        }
        }
        else {
            return NULL;
        }
    }
}

void *calcAlarm(void *arg)
{
    struct thread_data *data = arg;

    bool end_thread = false;
    bool initialization = true;
    bool alarm_state = false;
    bool warning_state = false;
    bool obstructed = false;

    // Hold timer: when an alarm/warning fires, keep it asserted for
    // ALARM_HOLD_TICKS more iterations so brief sensor dips don't drop the
    // siren. One tick = ~250 ms (see sleepForMs at end of loop).
    const int ALARM_HOLD_TICKS = 10;
    int hold_ticks_remaining = 0;

    bool temp_trigger, ir_trigger, smoke_trigger, CO_trigger, CO2_trigger;

    // Open the I2C bus once and configure the LED matrix.
    int i2c_fd = i2cOpenBus(I2CDRV_LINUX_BUS1, I2C_DEVICE_ADDRESS);
    if (i2c_fd >= 0) {
        i2cWriteCmd(i2c_fd, LED_REG_SYSTEM_SETUP);   // oscillator on
        i2cWriteCmd(i2c_fd, LED_REG_DISPLAY_SETUP);  // display on, no blink
        ledFill(i2c_fd, LED_PATTERN_OFF);
    }

    // Track the last pattern written so we only push to I2C when it changes
    // (avoids 250ms flicker and unnecessary bus traffic).
    enum { PAT_NONE, PAT_OFF, PAT_WARNING, PAT_ALARM } last_pattern = PAT_NONE;

    while(1) {
        pthread_mutex_lock(&data->mutexControl);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&data->mutexControl);

        watchdogKick(data, WATCHDOG_IDX_ALARM);

        if(end_thread == false) {
            if(initialization == true) {
                initialization = false;
                sleepForMs(1000);
            }
            else {
                pthread_mutex_lock(&data->mutexAlarm);
                {
                    temp_trigger  = data->alarm_temp;
                    ir_trigger    = data->alarm_IR;
                    smoke_trigger = data->alarm_smoke;
                    CO2_trigger   = data->alarm_CO2;
                    CO_trigger    = data->alarm_CO;
                }
                pthread_mutex_unlock(&data->mutexAlarm);

                // Count how many gas/temp sensors are currently above their
                // threshold. IR is handled separately (obstruction check).
                int trigger_count = 0;
                if (CO_trigger)    trigger_count++;
                if (temp_trigger)  trigger_count++;
                if (CO2_trigger)   trigger_count++;
                if (smoke_trigger) trigger_count++;

                // Latch new alarm/warning conditions.
                // CO alone is enough to raise a full alarm.
                bool fresh_alarm   = (CO_trigger) || (trigger_count >= 2);
                bool fresh_warning = (!fresh_alarm) && (trigger_count == 1);

                if (fresh_alarm) {
                    alarm_state = true;
                    warning_state = false;
                    hold_ticks_remaining = ALARM_HOLD_TICKS;
                } else if (fresh_warning && !alarm_state) {
                    warning_state = true;
                    hold_ticks_remaining = ALARM_HOLD_TICKS;
                } else if (hold_ticks_remaining > 0) {
                    // Nothing currently triggering; decay the hold timer.
                    hold_ticks_remaining--;
                    if (hold_ticks_remaining == 0) {
                        alarm_state = false;
                        warning_state = false;
                    }
                }

                // Obstructed: only IR is firing and no fire alarm is latched.
                obstructed = (trigger_count == 0) && ir_trigger && !alarm_state;
                if (obstructed) {
                    warning_state = false;
                }

                pthread_mutex_lock(&data->mutexAlarm);
                {
                    data->general_alarm = alarm_state;
                    data->obstructed_alarm = obstructed;
                }
                pthread_mutex_unlock(&data->mutexAlarm);

                if((((warning_state == true) || (obstructed == true)) && (alarm_state == false))) {
                    if (last_pattern != PAT_WARNING) {
                        ledWritePattern(i2c_fd, LED_PATTERN_WARNING);
                        last_pattern = PAT_WARNING;
                    }
                }

                if((warning_state == true) && (alarm_state == false) && (obstructed == false)) {
                        if(CO2_trigger == true) {
                            printf("\n");
                            printf("WARNING: CO2 levels above threshold!\n");
                        }
                        else
                        if(smoke_trigger == true) {
                            printf("\n");
                            printf("WARNING: Smoke / Fammable Gas levels above threshold!\n");
                        }
                        else
                        if(temp_trigger == true) {
                            printf("\n");
                            printf("WARNING: Temperature above threshold!\n");
                        }
                }
                else if(alarm_state == true) {
                    if(CO_trigger == true) {
                        printf("\n");
                        printf("ALARM: CO LEVEL HIGHER THAN THRESHOLD, LEAVE AREA IMMEIDIATELY!");
                        printf("\n");
                    }
                    else {
                        printf("\n");
                        printf("ALARM: TWO OR MORE SENSORS ABOVE THRESHOLD: ");
                        if(CO2_trigger == true) {
                                printf("CO2 SENSOR, ");
                            }
                        if(smoke_trigger == true) {
                                printf("SMOKE/GAS SENSOR, ");
                            }
                        if(temp_trigger == true) {
                                printf("TEMPERATURE SENSOR, ");
                            }        
                        printf("\n");
                    }

                    if (last_pattern != PAT_ALARM) {
                        ledFill(i2c_fd, LED_PATTERN_FULL);
                        last_pattern = PAT_ALARM;
                    }
                }
                else if (alarm_state == false && warning_state == false && obstructed == false) {
                    if (last_pattern != PAT_OFF) {
                        ledFill(i2c_fd, LED_PATTERN_OFF);
                        last_pattern = PAT_OFF;
                    }
                }

                sleepForMs(250);
            }
        }
        else {
            if (i2c_fd >= 0) {
                ledFill(i2c_fd, LED_PATTERN_OFF);
                close(i2c_fd);
            }
            return NULL;
        }
    }
}
