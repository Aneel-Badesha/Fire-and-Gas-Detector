# Multi Purpose Fire and Gas Detector

An embedded fire and gas detection system built in C on Debian Linux and cross-compiled for deployment on a BeagleBone Green. The system provides real-time monitoring and alerting for fire and gas hazards.

## Hardware Components

### Target Platform
- **BeagleBone Green**: Main processing unit and GPIO controller

### Sensors (5 total)
| Sensor | Model | ADC Channel |
|--------|-------|-------------|
| Temperature | TMP36 | Channel 0 |
| IR | TCRT5000 | Channel 1 |
| CO2 | MQ-135 | Channel 2 |
| Smoke | MQ-2 | Channel 3 |
| CO | MQ-7 | Channel 5 |

All sensors connect to the BeagleBone's on-board ADC inputs (1.8V reference, 12-bit resolution) and are read through the Linux IIO interface (`/sys/bus/iio/devices/iio:device0/in_voltageN_raw`).

### Display & Alerting
- **8x8 LED Matrix**: Visual alarm display connected via I2C (pins P9_17 / P9_18, HT16K33 at address `0x70`)

### User Input
- **USER button**: On-board button for graceful shutdown (`/sys/class/gpio/gpio72/value`)

## Software Architecture

### Development Environment
- **Language**: C17, POSIX threads
- **Toolchain**: `arm-linux-gnueabihf-gcc` cross-compiler (auto-detected; falls back to native `gcc`)
- **Target**: ARMv7 (BeagleBone Green)

### Threading Model
10 POSIX threads, each monitored by the watchdog:

| Thread | Role | Poll interval |
|--------|------|---------------|
| `readTemperature` | ADC → EMA → `value[SENSOR_TEMP]` | 500 ms |
| `readIR` | ADC → EMA → `ema_ir` | 50 ms |
| `readCO` | ADC → EMA → `value[SENSOR_CO]` | 500 ms |
| `readCO2` | ADC → EMA → `value[SENSOR_CO2]` | 500 ms |
| `readSmoke` | ADC → EMA → `value[SENSOR_SMOKE]` | 1000 ms |
| `calculateStatus` | Publishes `ema_ir` → `value[SENSOR_IR]`, sets `alarm[]` flags | 500 ms |
| `calcAlarm` | Reads `alarm[]`, drives LED matrix, prints messages | 250 ms |
| `displayOutput` | Prints live sensor readings | 2500 ms |
| `watchdogMonitor` | Checks all thread heartbeats, shuts down on timeout | 1000 ms |
| `exitProgram` | Polls USER button, triggers shutdown | 100 ms |

### Shared State (`struct thread_data`)
All inter-thread data lives in a single `struct thread_data` passed to every thread:

```c
struct thread_data {
    bool   end_all_threads;                    // g_mutex_control
    time_t watchdog_kicks[THREAD_COUNT];       // g_mutex_watchdog
    float  value[SENSOR_COUNT];                // g_mutex_sensor[i]
    float  ema_ir;                             // g_mutex_sensor[SENSOR_IR]
    bool   alarm[SENSOR_COUNT];                // g_mutex_alarm
    bool   general_alarm;                      // g_mutex_alarm
    bool   obstructed_alarm;                   // g_mutex_alarm
};
```

Mutexes and thread IDs are globals (defined in `common.c`):
- `g_mutex_control` — protects `end_all_threads`
- `g_mutex_sensor[SENSOR_COUNT]` — one per sensor, protects `value[i]` and `ema_ir`
- `g_mutex_alarm` — protects all `alarm[]` flags and aggregate alarm state
- `g_mutex_watchdog` — protects `watchdog_kicks[]`
- `g_tid[THREAD_COUNT]` — all thread IDs

### Alarm Logic
`calculateStatus` evaluates each sensor against its threshold (defined as `*POINT` macros in [common.h](common.h)) and sets `alarm[i]`. `calcAlarm` then aggregates:

- **General alarm**: CO alone, or 2+ non-IR sensors above threshold
- **Warning**: exactly 1 non-IR sensor above threshold
- **Obstructed**: only IR triggered with no other alarms (sensor blocked)

A 10-tick hold timer (~2.5 s) prevents brief sensor dips from dropping an active alarm.

### Sensor Processing
All five sensors share a single generic loop (`runSensorLoop` in [sensor.c](sensor.c)). Each reading is filtered with an exponential moving average (EMA, α=0.6). The temperature sensor applies the TMP36 transfer function (raw → °C); all others convert raw ADC counts to volts.

### Watchdog
Every thread kicks a timestamp each iteration. `watchdogMonitor` checks all 10 timestamps every second and triggers shutdown if any thread exceeds the 5-second timeout.

## Alarm Thresholds

| Sensor | Threshold | Condition |
|--------|-----------|-----------|
| IR | > 1.75 V | Full alarm |
| CO | > 0.9 V | Full alarm (alone) |
| CO2 | > 0.9 V | Warning / alarm |
| Smoke | > 0.65 V | Warning / alarm |
| Temperature | > 27 °C | Warning / alarm |

## Getting Started

### Prerequisites
- Linux development environment (native or WSL)
- `arm-linux-gnueabihf-gcc` cross-compilation toolchain (optional; falls back to `gcc`)
- BeagleBone Green with sensors and LED matrix wired up

### Hardware Setup
1. Connect sensors to ADC channels 0, 1, 2, 3, 5 (see table above)
2. Wire the 8x8 LED matrix to I2C pins P9_17 / P9_18
3. Ensure proper power supply for all components

### Build & Deploy

```
make          # compile; binary output to build/main
make clean    # remove build/ directory
```

1. Install the cross-compiler: `sudo apt install gcc-arm-linux-gnueabihf`
2. Run `make` — binary at `build/main`
3. Transfer to the BeagleBone: `scp build/main user@beaglebone:~`
4. Run on target; press the USER button to shut down cleanly
