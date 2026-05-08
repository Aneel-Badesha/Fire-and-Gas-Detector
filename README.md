# Multi Purpose Fire and Gas Detector

An embedded fire and gas detection system built with C programming language on Debian Linux and cross-compiled for deployment on a BeagleBone Green. This system provides real-time monitoring and alerting capabilities for fire and gas hazards.

## Overview

This project implements a comprehensive fire and gas detection system using multiple sensors for redundant safety monitoring. The system features visual alarm capabilities and is designed for reliable operation in safety-critical environments.

## Hardware Components
 
### Target Platform
- **BeagleBone Green**: Main processing unit and GPIO controller 

### Sensors (5 total)
- IR Sensor
- CO Sensor
- CO2 Sensor
- Smoke Sensor
- Temperature Sensor
- Connected via the BeagleBone's on-board A2D (ADC) inputs, read through the Linux IIO interface (`/sys/bus/iio/devices/iio:device0/in_voltageN_raw`)

### Display & Alerting
- **8x8 LED Matrix**: Visual alarm display connected via I2C interface (pins P9_17/P9_18)
- Provides clear visual indication of alarm states

### User Input
- **USER button**: On-board button used to gracefully shut down the system

## Software Architecture

### Development Environment
- **Development OS**: Debian Linux
- **Programming Language**: C (C17, POSIX threads)
- **Toolchain**: `arm-linux-gnueabihf-gcc` cross-compiler
- **Deployment**: Cross-compiled for ARM architecture (BeagleBone Green)

### Key Features
- Real-time multi-threaded sensor monitoring (8 pthreads: user, temperature, IR, air sensors, status, alarm, output, watchdog)
- Rolling-average filtering on temperature and IR readings
- I2C communication for LED matrix control
- Alarm state management with warning, general, and obstructed-sensor states
- Mutex-protected shared state between threads
- **Watchdog timer**: Monitors all worker threads; automatically triggers shutdown if any thread becomes unresponsive

## System Design

### Sensor Integration
- Multiple sensor inputs for enhanced reliability
- Analog sensor interfacing via the BeagleBone's 7-channel A2D (1.8V reference, 12-bit resolution)
- Configurable threshold detection (see `*POINT` macros in [common.h](common.h))

### Alert System
- 8x8 LED matrix for visual warnings
- I2C protocol for display communication
- Customizable alarm patterns

### Safety Features
- Multi-sensor redundancy
- Real-time monitoring
- Fail-safe alarm mechanisms
- **Thread watchdog timer**: Each of the 6 worker threads periodically updates a timestamp. A dedicated watchdog monitor checks all timestamps every second and triggers system shutdown if any thread fails to respond within 5 seconds, ensuring system reliability in case of thread hangs or crashes

## Development Workflow

1. **Development**: Code written and tested on Debian Linux
2. **Cross-compilation**: Built for ARM target architecture
3. **Deployment**: Transferred and executed on BeagleBone Green
4. **Testing**: Hardware-in-the-loop verification

## Getting Started

### Prerequisites
- Linux development environment
- `arm-linux-gnueabihf-gcc` cross-compilation toolchain
- BeagleBone Green with required sensors
- 8x8 LED matrix display

### Hardware Setup
1. Connect the 5 sensors to the BeagleBone's A2D input channels (channels 0, 1, 2, 3, 5 — see header comment in [sensor.c](sensor.c))
2. Wire the 8x8 LED matrix to the I2C bus (pins P9_17 / P9_18)
3. Ensure proper power supply for all components

### Build & Deployment
The [Makefile](Makefile) targets the BeagleBone Green and stages the binary for deployment:

```
make          # cross-compile and copy to $(HOME)/cmpt433/public/myApps/main
make clean    # remove build artifacts
```

1. Cross-compile the C source on the host
2. Transfer the executable to the BeagleBone Green (via the shared `myApps` directory or `scp`)
3. Ensure I2C access is configured (`main.c` calls `config-pin` for P9_17/P9_18 at startup)
4. Run the binary on the target; press the USER button to shut down

## Technical Specifications 

- **Target Architecture**: ARMv7 (BeagleBone Green)
- **Communication Protocols**: A2D (via Linux IIO), I2C, GPIO (USER button)
- **Sensor Count**: 5 detection units
- **Display**: 8x8 LED matrix (I2C)
- **Concurrency**: 8 POSIX threads with mutex-protected shared state
- **Watchdog**: 5-second per-thread timeout, checked every second
- **Real-time Operation**: Continuous monitoring

This embedded system is a robust device for fire and gas detection applications with reliable hardware interfacing and cross-platform development capabilities.
