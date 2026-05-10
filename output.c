#include "output.h"

#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

static const unsigned char LED_ROW_REGS[8] = {
    LED_REG_ROW0, LED_REG_ROW1, LED_REG_ROW2, LED_REG_ROW3,
    LED_REG_ROW4, LED_REG_ROW5, LED_REG_ROW6, LED_REG_ROW7,
};

// 8-row pattern rendered as ! used for the warning state
static const unsigned char LED_PATTERN_WARNING[8] = {
    LED_PATTERN_EXCLAMATION, LED_PATTERN_EXCLAMATION, LED_PATTERN_EXCLAMATION,
    LED_PATTERN_EXCLAMATION, LED_PATTERN_EXCLAMATION, LED_PATTERN_OFF,
    LED_PATTERN_OFF,         LED_PATTERN_EXCLAMATION,
};

// Opens an I2C bus and binds it to the given slave address returns fd or -1 on failure
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

// Writes a single-byte command to the I2C device
int i2cWriteCmd(int fd, unsigned char cmd)
{
    if (write(fd, &cmd, 1) != 1) {
        fprintf(stderr, "WARN: i2c cmd %#x failed: %s\n", cmd, strerror(errno));
        return -1;
    }
    return 0;
}

// Writes a value to a register on the I2C device
int i2cWriteReg(int fd, unsigned char reg, unsigned char value)
{
    unsigned char buf[2] = { reg, value };
    if (write(fd, buf, 2) != 2) {
        fprintf(stderr, "WARN: i2c reg %#x=%#x failed: %s\n", reg, value, strerror(errno));
        return -1;
    }
    return 0;
}

// Writes an 8-row bitmap pattern to the LED matrix over I2C
static void ledWritePattern(int fd, const unsigned char rows[8])
{
    if (fd < 0) return;
    for (int i = 0; i < 8; i++) {
        i2cWriteReg(fd, LED_ROW_REGS[i], rows[i]);
    }
}

// Fills all 8 rows of the LED matrix with the same byte value
static void ledFill(int fd, unsigned char value)
{
    if (fd < 0) return;
    for (int i = 0; i < 8; i++) {
        i2cWriteReg(fd, LED_ROW_REGS[i], value);
    }
}

// Returns true if value exceeds the threshold
bool thresholdHigh(float value, float tpoint) {
    return value > tpoint;
}

// Prints sensor readings to stdout every 2.5s or warns if the board is obstructed
void *displayOutput(void *arg)
{
    struct thread_data *data = arg;
    bool end_thread = false;
    bool initialization = true;

    bool alarm_state = false;
    bool obstructed = false;

    while (1) {
        pthread_mutex_lock(&g_mutex_control);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&g_mutex_control);

        watchdogKick(data, WATCHDOG_IDX_OUTPUT);

        pthread_mutex_lock(&g_mutex_alarm);
        {
            alarm_state = data->general_alarm;
            obstructed  = data->obstructed_alarm;
        }
        pthread_mutex_unlock(&g_mutex_alarm);

        if (end_thread == false) {
            if (initialization == true) {
                initialization = false;
                sleepForMs(1000);
            } else {
                if ((obstructed == false) || (alarm_state == true)) {
                    float value_co, value_co2, value_smoke, value_temp, value_ir;

                    pthread_mutex_lock(&g_mutex_sensor[SENSOR_CO]);
                    { value_co = data->value[SENSOR_CO]; }
                    pthread_mutex_unlock(&g_mutex_sensor[SENSOR_CO]);

                    pthread_mutex_lock(&g_mutex_sensor[SENSOR_CO2]);
                    { value_co2 = data->value[SENSOR_CO2]; }
                    pthread_mutex_unlock(&g_mutex_sensor[SENSOR_CO2]);

                    pthread_mutex_lock(&g_mutex_sensor[SENSOR_SMOKE]);
                    { value_smoke = data->value[SENSOR_SMOKE]; }
                    pthread_mutex_unlock(&g_mutex_sensor[SENSOR_SMOKE]);

                    pthread_mutex_lock(&g_mutex_sensor[SENSOR_TEMP]);
                    { value_temp = data->value[SENSOR_TEMP]; }
                    pthread_mutex_unlock(&g_mutex_sensor[SENSOR_TEMP]);

                    pthread_mutex_lock(&g_mutex_sensor[SENSOR_IR]);
                    { value_ir = data->value[SENSOR_IR]; }
                    pthread_mutex_unlock(&g_mutex_sensor[SENSOR_IR]);

                    printf("\nTemperature: %.2f, IR Voltage: %.2f \n", value_temp, value_ir);
                    printf("CO Voltage: %.2f, CO2 Voltage: %.2f, Smoke Voltage: %.2f \n\n",
                           value_co, value_co2, value_smoke);
                } else if ((obstructed == true) && (alarm_state == false)) {
                    printf("WARNING: Board likely obstructed!\n");
                    printf("\n");
                }

                sleepForMs(2500);
            }
        } else {
            return NULL;
        }
    }
}

// Ccompares sensor EMA values against thresholds and sets per-sensor alarm flags every 500ms
void *calculateStatus(void *arg)
{
    struct thread_data *data = arg;
    bool end_thread = false;
    bool initialization = true;

    while (1) {
        pthread_mutex_lock(&g_mutex_control);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&g_mutex_control);

        watchdogKick(data, WATCHDOG_IDX_STATUS);

        if (end_thread == false) {
            if (initialization == false) {
                float value_co, value_co2, value_smoke, value_temp;

                pthread_mutex_lock(&g_mutex_sensor[SENSOR_CO]);
                { value_co = data->value[SENSOR_CO]; }
                pthread_mutex_unlock(&g_mutex_sensor[SENSOR_CO]);

                pthread_mutex_lock(&g_mutex_sensor[SENSOR_CO2]);
                { value_co2 = data->value[SENSOR_CO2]; }
                pthread_mutex_unlock(&g_mutex_sensor[SENSOR_CO2]);

                pthread_mutex_lock(&g_mutex_sensor[SENSOR_SMOKE]);
                { value_smoke = data->value[SENSOR_SMOKE]; }
                pthread_mutex_unlock(&g_mutex_sensor[SENSOR_SMOKE]);

                pthread_mutex_lock(&g_mutex_sensor[SENSOR_TEMP]);
                { value_temp = data->value[SENSOR_TEMP]; }
                pthread_mutex_unlock(&g_mutex_sensor[SENSOR_TEMP]);

                float value_ir_ema = 0.0f;
                pthread_mutex_lock(&g_mutex_sensor[SENSOR_IR]);
                {
                    value_ir_ema = data->ema_ir;
                    data->value[SENSOR_IR] = value_ir_ema;
                }
                pthread_mutex_unlock(&g_mutex_sensor[SENSOR_IR]);

                pthread_mutex_lock(&g_mutex_alarm);
                {
                    data->alarm[SENSOR_IR]    = thresholdHigh(value_ir_ema, IRPOINT);
                    data->alarm[SENSOR_TEMP]  = thresholdHigh(value_temp, TEMPPOINT);
                    data->alarm[SENSOR_CO]    = thresholdHigh(value_co, COPOINT);
                    data->alarm[SENSOR_CO2]   = thresholdHigh(value_co2, CO2POINT);
                    data->alarm[SENSOR_SMOKE] = thresholdHigh(value_smoke, SMOKEPOINT);
                }
                pthread_mutex_unlock(&g_mutex_alarm);

                sleepForMs(500);
            } else {
                initialization = false;
            }
        } else {
            return NULL;
        }
    }
}

// Aggregates alarm flags, drives the LED matrix, and prints alarm/warning messages every 250ms
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
    // siren. One tick = ~250 ms (see sleepForMs at end of loop)
    const int ALARM_HOLD_TICKS = 10;
    int hold_ticks_remaining = 0;

    bool trigger_temp, trigger_ir, trigger_smoke, trigger_co, trigger_co2;

    // Open the I2C bus once and configure the LED matrix
    int i2c_fd = i2cOpenBus(I2CDRV_LINUX_BUS1, I2C_DEVICE_ADDRESS);
    if (i2c_fd >= 0) {
        i2cWriteCmd(i2c_fd, LED_REG_SYSTEM_SETUP);   // oscillator on
        i2cWriteCmd(i2c_fd, LED_REG_DISPLAY_SETUP);  // display on, no blink
        ledFill(i2c_fd, LED_PATTERN_OFF);
    }

    // Track the last pattern written so we only push to I2C when it changes
    enum { PAT_NONE, PAT_OFF, PAT_WARNING, PAT_ALARM } last_pattern = PAT_NONE;

    while (1) {
        pthread_mutex_lock(&g_mutex_control);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&g_mutex_control);

        watchdogKick(data, WATCHDOG_IDX_ALARM);

        if (end_thread == false) {
            if (initialization == true) {
                initialization = false;
                sleepForMs(1000);
            } else {
                pthread_mutex_lock(&g_mutex_alarm);
                {
                    trigger_temp  = data->alarm[SENSOR_TEMP];
                    trigger_ir    = data->alarm[SENSOR_IR];
                    trigger_smoke = data->alarm[SENSOR_SMOKE];
                    trigger_co2   = data->alarm[SENSOR_CO2];
                    trigger_co    = data->alarm[SENSOR_CO];
                }
                pthread_mutex_unlock(&g_mutex_alarm);

                // Count how many gas/temp sensors are currently above their
                // threshold. IR is handled separately
                int trigger_count = 0;
                if (trigger_co)    trigger_count++;
                if (trigger_temp)  trigger_count++;
                if (trigger_co2)   trigger_count++;
                if (trigger_smoke) trigger_count++;

                // Latch new alarm/warning conditions
                // CO alone is enough to raise a full alarm
                bool fresh_alarm   = trigger_co || (trigger_count >= 2);
                bool fresh_warning = (!fresh_alarm) && (trigger_count == 1);

                if (fresh_alarm) {
                    alarm_state = true;
                    warning_state = false;
                    hold_ticks_remaining = ALARM_HOLD_TICKS;
                } else if (fresh_warning && !alarm_state) {
                    warning_state = true;
                    hold_ticks_remaining = ALARM_HOLD_TICKS;
                } else if (hold_ticks_remaining > 0) {
                    // Nothing currently triggering; decay the hold timer
                    hold_ticks_remaining--;
                    if (hold_ticks_remaining == 0) {
                        alarm_state = false;
                        warning_state = false;
                    }
                }

                // Obstructed: only IR is firing and no fire alarm is latched
                obstructed = (trigger_count == 0) && trigger_ir && !alarm_state;
                if (obstructed) {
                    warning_state = false;
                }

                pthread_mutex_lock(&g_mutex_alarm);
                {
                    data->general_alarm    = alarm_state;
                    data->obstructed_alarm = obstructed;
                }
                pthread_mutex_unlock(&g_mutex_alarm);

                if (((warning_state == true) || (obstructed == true)) && (alarm_state == false)) {
                    if (last_pattern != PAT_WARNING) {
                        ledWritePattern(i2c_fd, LED_PATTERN_WARNING);
                        last_pattern = PAT_WARNING;
                    }
                }

                if ((warning_state == true) && (alarm_state == false) && (obstructed == false)) {
                    if (trigger_co2) {
                        printf("\nWARNING: CO2 levels above threshold!\n");
                    } else if (trigger_smoke) {
                        printf("\nWARNING: Smoke / Flammable Gas levels above threshold!\n");
                    } else if (trigger_temp) {
                        printf("\nWARNING: Temperature above threshold!\n");
                    }
                } else if (alarm_state == true) {
                    if (trigger_co) {
                        printf("\nALARM: CO LEVEL HIGHER THAN THRESHOLD, LEAVE AREA IMMEDIATELY!\n");
                    } else {
                        printf("\nALARM: TWO OR MORE SENSORS ABOVE THRESHOLD: ");
                        if (trigger_co2)   printf("CO2 SENSOR, ");
                        if (trigger_smoke) printf("SMOKE/GAS SENSOR, ");
                        if (trigger_temp)  printf("TEMPERATURE SENSOR, ");
                        printf("\n");
                    }

                    if (last_pattern != PAT_ALARM) {
                        ledFill(i2c_fd, LED_PATTERN_FULL);
                        last_pattern = PAT_ALARM;
                    }
                } else if (alarm_state == false && warning_state == false && obstructed == false) {
                    if (last_pattern != PAT_OFF) {
                        ledFill(i2c_fd, LED_PATTERN_OFF);
                        last_pattern = PAT_OFF;
                    }
                }

                sleepForMs(250);
            }
        } else {
            if (i2c_fd >= 0) {
                ledFill(i2c_fd, LED_PATTERN_OFF);
                close(i2c_fd);
            }
            return NULL;
        }
    }
}
