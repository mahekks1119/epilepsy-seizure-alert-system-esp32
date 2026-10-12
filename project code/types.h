#pragma once
#include <stdint.h>

enum SensorStatus : uint8_t {
  SENSOR_PPG_OK = 1u << 0,
  SENSOR_IMU_OK = 1u << 1,
};

// One aggregated record produced every SENSOR_PUBLISH_MS by the sensor task.
struct SensorRecord {
  uint32_t tsMs;
  float heartRate;      // bpm, 0 = not available
  float spo2;           // percent, 0 = not available
  float accMagG;        // latest |a| in g
  float accDynPeakG;    // peak |(|a| - 1 g)| during this interval
  uint8_t vibrationSeen;
  uint8_t status;       // SensorStatus bits
};

enum AlarmReason : uint8_t {
  REASON_NONE = 0,
  REASON_LOW_SPO2,
  REASON_HIGH_HR,
  REASON_SEIZURE_MOTION,
  REASON_SENSOR_FAULT,
  REASON_COUNT
};

enum class MsgType : uint8_t { Telemetry, Alert, SystemReset };

// Message from the processing task to the comms task.
struct CommsMsg {
  MsgType type;
  float heartRate;
  float spo2;
  float accMagG;
  uint8_t vibration;
  AlarmReason reason;
};

inline const char *reasonText(AlarmReason r) {
  switch (r) {
    case REASON_LOW_SPO2: return "Low SpO2 Detected!";
    case REASON_HIGH_HR: return "High Heart Rate!";
    case REASON_SEIZURE_MOTION: return "Seizure-Like Movement Detected!";
    case REASON_SENSOR_FAULT: return "Sensor Fault!";
    default: return "OK";
  }
}
