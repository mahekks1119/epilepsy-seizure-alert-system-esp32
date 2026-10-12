// Processing and alert task: runs the detector, drives the buzzer, supervises the sensor task.
#include <Arduino.h>
#include "app_globals.h"
#include "config.h"
#include "detector.h"
#include "tasks.h"
#include "types.h"

static void postToComms(const CommsMsg &m, TickType_t wait) {
  if (xQueueSend(g_commsQueue, &m, wait) != pdTRUE) {
    Serial.println("[process] comms queue full, message dropped");
  }
}

static void postAlert(AlarmReason reason) {
  CommsMsg m{};
  m.type = MsgType::Alert;
  m.reason = reason;
  postToComms(m, pdMS_TO_TICKS(100));  // alerts are worth waiting briefly for
}

void processingTask(void *) {
  Detector detector;
  SensorRecord rec{};
  SensorRecord last{};
  bool haveRecord = false;

  uint32_t lastRecordMs = millis();
  uint32_t lastTelemetryMs = millis();
  uint32_t lastResetMs = 0;
  uint8_t vibThisSecond = 0;

  bool imuBad = false;
  uint32_t imuBadSinceMs = 0;
  bool faultReported = false;

  for (;;) {
    // 1) Reset requests (button ISR or Blynk app) arrive as task notifications.
    if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
      const uint32_t now = millis();
      if (now - lastResetMs > RESET_DEBOUNCE_MS) {
        lastResetMs = now;
        detector.reset();
        digitalWrite(PIN_BUZZER, LOW);
        Serial.println("[process] System reset.");
        CommsMsg m{};
        m.type = MsgType::SystemReset;
        postToComms(m, pdMS_TO_TICKS(100));
      }
    }

    // 2) Next record from the sensor task (blocks up to 100 ms, so reset stays responsive).
    if (xQueueReceive(g_sensorQueue, &rec, pdMS_TO_TICKS(100)) == pdTRUE) {
      lastRecordMs = millis();
      last = rec;
      haveRecord = true;
      if (rec.vibrationSeen) vibThisSecond = 1;

      const AlarmReason raised = detector.update(rec);
      if (raised != REASON_NONE) {
        digitalWrite(PIN_BUZZER, HIGH);
        Serial.printf("[process] ALARM: %s\n", reasonText(raised));
        postAlert(raised);
      }

      if (rec.status & SENSOR_IMU_OK) {
        imuBad = false;
      } else if (!imuBad) {
        imuBad = true;
        imuBadSinceMs = millis();
      }
    }

    // 3) Supervision: a stalled sensor task or a missing IMU is reported, never silent.
    const uint32_t now = millis();
    const bool stalled = (now - lastRecordMs) > SENSOR_STALL_MS;
    const bool imuFault = imuBad && (now - imuBadSinceMs) >= IMU_FAULT_MS;
    if ((stalled || imuFault) && !faultReported) {
      faultReported = true;
      Serial.println("[process] FAULT: sensor path not healthy");
      postAlert(REASON_SENSOR_FAULT);
    } else if (!stalled && !imuFault) {
      faultReported = false;
    }

    // 4) Telemetry at 1 Hz.
    if (haveRecord && now - lastTelemetryMs >= TELEMETRY_PERIOD_MS) {
      lastTelemetryMs = now;
      CommsMsg m{};
      m.type = MsgType::Telemetry;
      m.heartRate = last.heartRate;
      m.spo2 = last.spo2;
      m.accMagG = last.accMagG;
      m.vibration = vibThisSecond;
      vibThisSecond = 0;
      postToComms(m, 0);  // telemetry may be dropped; the next one replaces it

      Serial.printf("SpO2: %.1f%% | HR: %.1f bpm | Movement: %.2f g | Vibration: %d | %s\n",
                    last.spo2, last.heartRate, last.accMagG, m.vibration,
                    detector.alarmLatched() ? reasonText(detector.reason()) : "OK");
    }
  }
}
