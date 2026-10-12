// Generalized Epileptic Seizure Alert System - ESP32 + FreeRTOS
//
//   Sensors -> sensorTask -> [sensor queue] -> processingTask -> [comms queue] -> commsTask -> Blynk
//
// See README.md for wiring, libraries and tuning.

#include <Arduino.h>
#include <Wire.h>
#include "app_globals.h"
#include "config.h"
#include "tasks.h"
#include "types.h"

QueueHandle_t g_sensorQueue = nullptr;
QueueHandle_t g_commsQueue = nullptr;
TaskHandle_t g_processingTask = nullptr;

// Reset button ISR: only wakes the processing task; debouncing and the reset itself happen there.
void IRAM_ATTR onResetButton() {
  BaseType_t woken = pdFALSE;
  if (g_processingTask) vTaskNotifyGiveFromISR(g_processingTask, &woken);
  if (woken == pdTRUE) portYIELD_FROM_ISR();
}

static void fatalRestart(const char *msg) {
  Serial.println(msg);
  delay(1000);
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Seizure Alert System (ESP32 + FreeRTOS) starting...");

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(400000);

  pinMode(PIN_VIBRATION, INPUT_PULLDOWN);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_RESET_BUTTON, INPUT_PULLUP);

  g_sensorQueue = xQueueCreate(SENSOR_QUEUE_LEN, sizeof(SensorRecord));
  g_commsQueue = xQueueCreate(COMMS_QUEUE_LEN, sizeof(CommsMsg));
  if (!g_sensorQueue || !g_commsQueue) fatalRestart("Queue creation failed, restarting.");

  if (xTaskCreatePinnedToCore(sensorTask, "sensor", STACK_SENSOR, nullptr, PRIO_SENSOR, nullptr,
                              CORE_APP) != pdPASS ||
      xTaskCreatePinnedToCore(processingTask, "process", STACK_PROCESSING, nullptr, PRIO_PROCESSING,
                              &g_processingTask, CORE_APP) != pdPASS ||
      xTaskCreatePinnedToCore(commsTask, "comms", STACK_COMMS, nullptr, PRIO_COMMS, nullptr,
                              CORE_WIFI) != pdPASS) {
    fatalRestart("Task creation failed, restarting.");
  }

  // Attach only after the processing task handle exists.
  attachInterrupt(digitalPinToInterrupt(PIN_RESET_BUTTON), onResetButton, FALLING);
}

void loop() {
  // All work happens in the FreeRTOS tasks; the Arduino loop task is not needed.
  vTaskDelete(nullptr);
}
