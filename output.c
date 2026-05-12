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

// Opens an I2C bus and binds it to the given device address, returns fd or -1 on failure
int i2cOpenBus(const char *bus_path, int device_addr)
{
    int fd = open(bus_path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "WARN: cannot open %s: %s\n", bus_path, strerror(errno));
        return -1;
    }
    if (ioctl(fd, I2C_SLAVE, device_addr) < 0) {
        fprintf(stderr, "WARN: I2C_SLAVE %#x failed: %s\n", device_addr, strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

// Writes a single byte command to the I2C device
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
    bool alarm_state = false;
    bool obstructed = false;

    while (1) {
        pthread_mutex_lock(&g_mutex_control);
        {
            end_thread = data->end_all_threads;
        }
        pthread_mutex_unlock(&g_mutex_control);

        if (end_thread) return NULL;

        watchdogKick(data, WATCHDOG_IDX_OUTPUT);

        pthread_mutex_lock(&g_mutex_alarm);
        {
            alarm_state = data->general_alarm;
            obstructed  = data->obstructed_alarm;
        }
        pthread_mutex_unlock(&g_mutex_alarm);

        if (!obstructed || alarm_state) {
            float value_co, value_co2, value_smoke, value_temp, value_ir;

            pthread_mutex_lock(&g_mutex_sensor[SENSOR_CO]);
            value_co = data->value[SENSOR_CO];
            pthread_mutex_unlock(&g_mutex_sensor[SENSOR_CO]);

            pthread_mutex_lock(&g_mutex_sensor[SENSOR_CO2]);
            value_co2 = data->value[SENSOR_CO2];
            pthread_mutex_unlock(&g_mutex_sensor[SENSOR_CO2]);

            pthread_mutex_lock(&g_mutex_sensor[SENSOR_SMOKE]);
            value_smoke = data->value[SENSOR_SMOKE];
            pthread_mutex_unlock(&g_mutex_sensor[SENSOR_SMOKE]);

            pthread_mutex_lock(&g_mutex_sensor[SENSOR_TEMP]);
            value_temp = data->value[SENSOR_TEMP];
            pthread_mutex_unlock(&g_mutex_sensor[SENSOR_TEMP]);

            pthread_mutex_lock(&g_mutex_sensor[SENSOR_IR]);
            value_ir = data->value[SENSOR_IR];
            pthread_mutex_unlock(&g_mutex_sensor[SENSOR_IR]);

            printf("\nTemperature: %.2f, IR Voltage: %.2f \n", value_temp, value_ir);
            printf("CO Voltage: %.2f, CO2 Voltage: %.2f, Smoke Voltage: %.2f \n\n",
                   value_co, value_co2, value_smoke);
        } else {
            printf("WARNING: Board likely obstructed!\n\n");
        }

        sleepForMs(2500);
    }
}

// Evaluates thresholds, aggregates alarm flags, drives the LED matrix, and prints messages every 250ms
void *calcAlarm(void *arg)
{
    struct thread_data *data = arg;

    bool end_thread = false;
    bool alarm_state = false;
    bool warning_state = false;
    bool obstructed = false;

    // Hold timer: keep alarm/warning asserted for ALARM_HOLD_TICKS after sensors drop below threshold
    // One tick = ~250 ms
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

        if (end_thread) {
            if (i2c_fd >= 0) {
                ledFill(i2c_fd, LED_PATTERN_OFF);
                close(i2c_fd);
            }
            return NULL;
        }

        // Read sensor values and evaluate thresholds
        float value_co, value_co2, value_smoke, value_temp, value_ir;

        pthread_mutex_lock(&g_mutex_sensor[SENSOR_CO]);
        value_co = data->value[SENSOR_CO];
        pthread_mutex_unlock(&g_mutex_sensor[SENSOR_CO]);

        pthread_mutex_lock(&g_mutex_sensor[SENSOR_CO2]);
        value_co2 = data->value[SENSOR_CO2];
        pthread_mutex_unlock(&g_mutex_sensor[SENSOR_CO2]);

        pthread_mutex_lock(&g_mutex_sensor[SENSOR_SMOKE]);
        value_smoke = data->value[SENSOR_SMOKE];
        pthread_mutex_unlock(&g_mutex_sensor[SENSOR_SMOKE]);

        pthread_mutex_lock(&g_mutex_sensor[SENSOR_TEMP]);
        value_temp = data->value[SENSOR_TEMP];
        pthread_mutex_unlock(&g_mutex_sensor[SENSOR_TEMP]);

        pthread_mutex_lock(&g_mutex_sensor[SENSOR_IR]);
        value_ir = data->value[SENSOR_IR];
        pthread_mutex_unlock(&g_mutex_sensor[SENSOR_IR]);

        trigger_co    = thresholdHigh(value_co,    COPOINT);
        trigger_co2   = thresholdHigh(value_co2,   CO2POINT);
        trigger_smoke = thresholdHigh(value_smoke,  SMOKEPOINT);
        trigger_temp  = thresholdHigh(value_temp,  TEMPPOINT);
        trigger_ir    = thresholdHigh(value_ir,    IRPOINT);

        // Count how many gas/temp sensors are above threshold; IR is handled separately
        int trigger_count = 0;
        if (trigger_co)    trigger_count++;
        if (trigger_temp)  trigger_count++;
        if (trigger_co2)   trigger_count++;
        if (trigger_smoke) trigger_count++;

        // CO alone raises a full alarm; 2+ sensors also raise a full alarm
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
            hold_ticks_remaining--;
            if (hold_ticks_remaining == 0) {
                alarm_state = false;
                warning_state = false;
            }
        }

        // Obstructed: only IR is firing and no fire alarm is latched
        obstructed = (trigger_count == 0) && trigger_ir && !alarm_state;
        if (obstructed) warning_state = false;

        pthread_mutex_lock(&g_mutex_alarm);
        {
            data->general_alarm    = alarm_state;
            data->obstructed_alarm = obstructed;
        }
        pthread_mutex_unlock(&g_mutex_alarm);

        if ((warning_state || obstructed) && !alarm_state) {
            if (last_pattern != PAT_WARNING) {
                ledWritePattern(i2c_fd, LED_PATTERN_WARNING);
                last_pattern = PAT_WARNING;
            }
        }

        if (warning_state && !alarm_state && !obstructed) {
            if (trigger_co2)        printf("\nWARNING: CO2 levels above threshold!\n");
            else if (trigger_smoke) printf("\nWARNING: Smoke / Flammable Gas levels above threshold!\n");
            else if (trigger_temp)  printf("\nWARNING: Temperature above threshold!\n");
        } else if (alarm_state) {
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
        } else if (!alarm_state && !warning_state && !obstructed) {
            if (last_pattern != PAT_OFF) {
                ledFill(i2c_fd, LED_PATTERN_OFF);
                last_pattern = PAT_OFF;
            }
        }

        sleepForMs(250);
    }
}
