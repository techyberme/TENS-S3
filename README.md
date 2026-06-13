# TENS-S3 Functionalities Overview

This project implements a Transcutaneous Electrical Nerve Stimulation (TENS) device using an ESP32-S3. The system consists of multiple modules managing different hardware and software features. Below is a comprehensive list of the functionalities identified in the codebase. *(Note: Some features may still be in development or incomplete).*

## 1. System State & Channel Management (`tensOS`)
- **System State Machine**: Controls the device lifecycle (`ZERO`, `TIME`, `PROGRAM`, `INIT`, `FUNC`, `DONE`, `LOW_BATTERY`, `ERROR`).
- **Dual-Channel Control**: Tracks independent channel states (`RUNNING`, `STBY`, `RECOVER`), applied intensity levels, real-time current, and voltage feedback.
- **Session Control**: Enforces session duration and timeouts.
- **Watchdog Timer**: Ensures system stability and safety.

## 2. Power Delivery & Control (`boost_control`)
- **Boost Converter Regulation**: Controls voltage generation using a PWM signal (RC Filter) and reads feedback to maintain the target voltage margin.
- **DAC Communication**: Adjusts output amplitudes independently for Channels A and B using dual MCP4725 DACs over I2C.
- **Emergency Stop & Safety Diagnostics**: Detects system faults such as:
  - Overcurrent / Overvoltage
  - Open Circuit (Electrode Detachment)
  - High Impedance
  - Low Efficiency (System Saturation)
- **LED Indicator**: Integrates single LED strip (RMT) for visual status.

## 3. Stimulation Driver (`hbridge_driver`)
- **H-Bridge Output**: Generates the stimulation pulses using GPIOs.
- **Configurable Modes**: 
  - Continuous Mode: 4 kHz constant output.
  - Burst Mode: 4 kHz modulated at 100 Hz.
  - Off state.

## 4. Monitoring & Battery (`adc_monitor`)
- **Voltage & Current Sensing**: Reads ADC values from Channels A and B for closed-loop control.
- **Battery Management**: Calculates battery percentage and filters readings to avoid noise.
- **Charging Monitor**: Detects active charging and prevents operation during charge cycles.
- **ADC Calibration**: Supports precise measurements through calibration routines.

## 5. User Interface (`oled` & `buttons`)
- **OLED Display Interface**: Drives an OLED screen indicating UI states: Logo, Time Config, Program Config, Running, Detached Electrodes, and Battery warnings.
- **Physical Navigation**: Three-button input (UP, DOWN, OK) with debouncing.
- **Lock/Unlock States**: Features security lockouts (`LOCKED`, `UNLOCKING`, `UNLOCKED`) and a specific `DOCTOR_STATE`.

## 6. Audio Feedback (`buzzer`)
- **Buzzer Control**: Uses the ESP32 LEDC peripheral to generate UI beeps for button presses and alarm tones for error warnings.

## 7. Persistent Configuration (`settings`)
- **NVS Storage**: Saves critical configurations into Non-Volatile Storage to persist across reboots.
- **Saved Data**: Stores parameters such as the active Program, Session Duration, and 'Doctor' permissions.

## 8. Project Structure

The repository is organized as follows:

```text
TENS-S3/
├── src/                    # Main application source code
│   ├── main.c              # System orchestrator and entry point
│   ├── CMakeLists.txt      # Build configuration for src
│   └── README.md           # This functionalities overview
├── lib/                    # Core project-specific libraries
│   ├── adc_monitor/        # ADC, battery, and charging monitoring
│   ├── boost_control/      # Boost converter, DAC, and safety logic
│   ├── buttons/            # Hardware button input and debouncing
│   ├── buzzer/             # Audio feedback control
│   ├── hbridge_driver/     # H-bridge stimulation output control
│   ├── oled/               # OLED screen UI management
│   ├── settings/           # Non-volatile storage for user preferences
│   └── tensOS/             # Core state machine and channel manager
├── components/             # External dependencies and IDF components
│   ├── led_strip/          # RGB LED strip driver
│   ├── u8g2/               # Display graphics library
│   └── u8g2-hal-esp-idf/   # Hardware abstraction for u8g2 on ESP32
├── principales/            # Alternate or legacy main files for testing
├── include/                # Global headers
├── test/                   # Unit tests
└── platformio.ini          # PlatformIO project configuration
```
