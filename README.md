# Generalized Epileptic Seizure Alert System (ESP32 + FreeRTOS)

ESP32 firmware using FreeRTOS for real-time IMU, heart rate and SpO2 monitoring, with Blynk-based WiFi alerts for remote patient monitoring.

> **Safety statement.** Academic prototype. Not a medical device, no clinical validation. Do not rely on it as the only means of supervising a person with epilepsy.

## Architecture

```
MAX30100 + MPU6050 + vibration sensor
        |
  sensorTask (core 1, prio 3)      reads I2C every 10 ms, aggregates a record every 100 ms
        |  sensor queue (SensorRecord)
  processingTask (core 1, prio 2)  detector, buzzer, supervision, 1 Hz telemetry
        |  comms queue (CommsMsg)
  commsTask (core 0, prio 1)       WiFi + Blynk, non-blocking reconnect
```

Design points:
- **One owner per resource.** Only `sensorTask` touches I2C (no bus mutex needed). Only `commsTask` calls Blynk (the library is not thread-safe).
- **Reset button is interrupt-driven.** The ISR only notifies `processingTask` (`vTaskNotifyGiveFromISR`); debouncing and the reset happen in the task. The Blynk V5 button uses the same path.
- **No silent hangs.** A missing sensor is retried every 5 s and reported as unavailable. A stalled sensor task or a missing IMU raises a fault alert.
- **Alerts survive offline periods.** `commsTask` keeps undelivered alerts until Blynk is connected.
- **Detection logic is separate** (`detector.cpp`, no Arduino or FreeRTOS calls) and is unit-tested on a PC.

## Fixes compared with the original single-loop code

| Original | Now |
|---|---|
| Movement threshold 40000 on an int16 (max 32767) could never fire | Thresholds in g; compile-time check against the sensor range |
| Returned early on invalid MAX30100 data, skipping motion checks | Vitals and motion are judged independently |
| Single-sample decisions | SpO2 and HR must stay out of range for 5 s; motion needs 10 jerky records in a 3 s window |
| Blocking WiFi connect and `while(1)` on sensor failure | Non-blocking, retry and fault reporting |
| WiFi password and Blynk token in source | `secrets.h` (git-ignored) |

## Hardware and pins

| Function | ESP32 pin |
|---|---|
| I2C SDA / SCL (MAX30100, MPU6050) | GPIO 21 / 22 |
| Vibration sensor (digital, HIGH = vibration) | GPIO 25 |
| Buzzer (active, HIGH = on) | GPIO 27 |
| Reset button to GND | GPIO 32 |

## Build

1. Arduino IDE with the **esp32 by Espressif** board package.
2. Libraries: **MAX30100lib** (OXullo Intersecans), **MPU6050** (Electronic Cats), **Blynk**.
3. Copy `secrets.h.example` to `secrets.h` and fill it in.
4. Select your ESP32 board, upload, open the Serial Monitor at 115200 baud.

## Blynk setup

| Datastream | Meaning |
|---|---|
| V0 | SpO2 (%) |
| V1 | Heart rate (bpm) |
| V2 | Movement magnitude (g; was raw counts in the old version, so adjust the widget range) |
| V3 | Vibration (0/1) |
| V4 | Alert text |
| V5 | Reset button (switch/button widget, optional) |

For phone push notifications, create an event with the code `seizure_alert` in the Blynk console.

## Tuning

All thresholds are in `config.h`. They are initial defaults, not validated values. Tune them against recorded data from healthy volunteers performing simulated movements.

## Tests

```
cd tests
g++ -std=c++11 -Wall -I.. ../detector.cpp host_test_detector.cpp -o host_test && ./host_test
```

## Known limitations

- Heart rate and SpO2 from the MAX30100 degrade with motion, which is when a seizure happens, so the motion path carries the main detection load.
- False alarm and miss rates have not been characterised.
- Requires WiFi and a Blynk connection for remote alerts; the buzzer works without them.
- Power: with WiFi on, an ESP32 draws on the order of 100 mA or more on average (estimate, not measured), so battery life is far shorter than a Bluetooth-based design. Measure before quoting runtime.
