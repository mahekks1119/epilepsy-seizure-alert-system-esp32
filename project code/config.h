#pragma once
// All tunable values live here. Thresholds are initial defaults and are NOT clinically validated.
#include <stdint.h>

// ---- Pins (same as the original ESP32 build) ----
constexpr int PIN_SDA = 21;
constexpr int PIN_SCL = 22;
constexpr int PIN_VIBRATION = 25;     // digital vibration sensor, HIGH = vibration
constexpr int PIN_BUZZER = 27;        // active buzzer, HIGH = on
constexpr int PIN_RESET_BUTTON = 32;  // to GND, internal pull-up, falling-edge interrupt

// ---- Sensor sampling ----
constexpr uint32_t SENSOR_PERIOD_MS = 10;    // 100 Hz loop (MAX30100 needs frequent update())
constexpr uint32_t SENSOR_PUBLISH_MS = 100;  // one aggregated record to the queue every 100 ms
constexpr uint32_t SENSOR_REINIT_MS = 5000;  // retry period for a sensor that failed to init
constexpr float ACCEL_LSB_PER_G = 8192.0f;   // MPU6050 at +/-4 g

// ---- Detection thresholds ----
constexpr float SPO2_ALARM_BELOW = 90.0f;        // percent
constexpr uint32_t SPO2_SUSTAIN_MS = 5000;       // must stay low this long
constexpr float HR_ALARM_ABOVE = 120.0f;         // bpm
constexpr uint32_t HR_SUSTAIN_MS = 5000;
constexpr float MOTION_THRESHOLD_G = 1.5f;       // peak |(|a| - 1 g)| counted as a jerk
constexpr int JERK_WINDOW_RECORDS = 30;          // 30 records x 100 ms = 3 s sliding window
constexpr int JERK_MIN_COUNT = 10;               // jerky records needed inside the window
constexpr bool REQUIRE_VIBRATION_FOR_MOTION = true;  // as in the original: motion AND vibration

// ---- Supervision / reporting ----
constexpr uint32_t TELEMETRY_PERIOD_MS = 1000;
constexpr uint32_t RESET_DEBOUNCE_MS = 300;
constexpr uint32_t SENSOR_STALL_MS = 2000;   // no record from the sensor task -> fault
constexpr uint32_t IMU_FAULT_MS = 10000;     // IMU missing this long -> fault

// ---- Comms ----
constexpr uint32_t WIFI_RETRY_MS = 10000;
constexpr uint32_t BLYNK_RETRY_MS = 10000;
constexpr uint32_t BLYNK_CONNECT_TIMEOUT_MS = 3000;
constexpr uint32_t COMMS_LOOP_MS = 20;
#define BLYNK_EVENT_NAME "seizure_alert"     // event code created in the Blynk console

// ---- FreeRTOS ----
constexpr int SENSOR_QUEUE_LEN = 20;
constexpr int COMMS_QUEUE_LEN = 10;
constexpr uint32_t STACK_SENSOR = 4096;      // bytes on ESP32
constexpr uint32_t STACK_PROCESSING = 4096;
constexpr uint32_t STACK_COMMS = 8192;
constexpr int PRIO_SENSOR = 3;
constexpr int PRIO_PROCESSING = 2;
constexpr int PRIO_COMMS = 1;
constexpr int CORE_APP = 1;                  // sensing + processing
constexpr int CORE_WIFI = 0;                 // WiFi/Blynk next to the WiFi stack

// ---- Compile-time sanity checks (the original threshold of 40000 could never fire on an int16) ----
static_assert(JERK_MIN_COUNT <= JERK_WINDOW_RECORDS, "JERK_MIN_COUNT must fit in the window");
static_assert(SENSOR_PUBLISH_MS % SENSOR_PERIOD_MS == 0, "publish period must be a multiple of the sensor period");
static_assert(MOTION_THRESHOLD_G < 4.0f, "motion threshold exceeds the +/-4 g sensor range");
