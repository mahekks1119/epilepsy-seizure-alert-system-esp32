#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

extern QueueHandle_t g_sensorQueue;    // SensorRecord: sensor task -> processing task
extern QueueHandle_t g_commsQueue;     // CommsMsg: processing task -> comms task
extern TaskHandle_t g_processingTask;  // target of reset notifications (button ISR, Blynk)
