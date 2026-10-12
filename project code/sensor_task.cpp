// Sensor acquisition task: the only code that touches I2C, so no bus mutex is needed.
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <MPU6050.h>
#include "MAX30100_PulseOximeter.h"
#include "app_globals.h"
#include "config.h"
#include "tasks.h"
#include "types.h"

static PulseOximeter pox;
static MPU6050 mpu;

static bool initPpg() { return pox.begin(); }

static bool initImu() {
  mpu.initialize();
  if (!mpu.testConnection()) return false;
  mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_4);  // +/-4 g -> 8192 LSB per g
  return true;
}

void sensorTask(void *) {
  bool ppgOk = false;
  bool imuOk = false;
  bool firstInit = true;
  uint32_t lastInitMs = 0;

  float lastMagG = 1.0f;
  float peakDynG = 0.0f;
  uint8_t vibrationSeen = 0;
  uint32_t ticks = 0;
  constexpr uint32_t ticksPerRecord = SENSOR_PUBLISH_MS / SENSOR_PERIOD_MS;

  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    const uint32_t now = millis();

    // A missing sensor never blocks the system: report it as unavailable and retry later.
    if ((!ppgOk || !imuOk) && (firstInit || now - lastInitMs >= SENSOR_REINIT_MS)) {
      firstInit = false;
      lastInitMs = now;
      if (!ppgOk) {
        ppgOk = initPpg();
        Serial.println(ppgOk ? "[sensor] MAX30100 ready" : "[sensor] MAX30100 init failed, will retry");
      }
      if (!imuOk) {
        imuOk = initImu();
        Serial.println(imuOk ? "[sensor] MPU6050 ready" : "[sensor] MPU6050 init failed, will retry");
      }
    }

    if (ppgOk) pox.update();  // must be called often; drains the MAX30100 FIFO

    if (imuOk) {
      int16_t ax, ay, az;
      mpu.getAcceleration(&ax, &ay, &az);
      const float gx = ax / ACCEL_LSB_PER_G;
      const float gy = ay / ACCEL_LSB_PER_G;
      const float gz = az / ACCEL_LSB_PER_G;
      const float mag = sqrtf(gx * gx + gy * gy + gz * gz);
      const float dyn = fabsf(mag - 1.0f);  // remove gravity
      lastMagG = mag;
      if (dyn > peakDynG) peakDynG = dyn;
    }

    if (digitalRead(PIN_VIBRATION) == HIGH) vibrationSeen = 1;

    if (++ticks >= ticksPerRecord) {
      ticks = 0;
      SensorRecord rec{};
      rec.tsMs = now;
      float hr = ppgOk ? pox.getHeartRate() : 0.0f;
      float spo2 = ppgOk ? (float)pox.getSpO2() : 0.0f;
      rec.heartRate = isnan(hr) ? 0.0f : hr;
      rec.spo2 = isnan(spo2) ? 0.0f : spo2;
      rec.accMagG = lastMagG;
      rec.accDynPeakG = peakDynG;
      rec.vibrationSeen = vibrationSeen;
      rec.status = (uint8_t)((ppgOk ? SENSOR_PPG_OK : 0) | (imuOk ? SENSOR_IMU_OK : 0));

      // Never block the sampling loop: if the queue is full, drop the oldest record.
      if (xQueueSend(g_sensorQueue, &rec, 0) != pdTRUE) {
        SensorRecord dropped;
        xQueueReceive(g_sensorQueue, &dropped, 0);
        xQueueSend(g_sensorQueue, &rec, 0);
      }
      peakDynG = 0.0f;
      vibrationSeen = 0;
    }

    vTaskDelayUntil(&wake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
  }
}
