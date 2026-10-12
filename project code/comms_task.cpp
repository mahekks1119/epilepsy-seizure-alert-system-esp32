// WiFi / Blynk communication task.
// Only this file includes the Blynk library (it defines the global Blynk object in its header),
// and only this task calls Blynk functions, because the library is not thread-safe.
#include "secrets.h"  // must come before the Blynk header (template id, name, token)
#include <Arduino.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "app_globals.h"
#include "config.h"
#include "tasks.h"
#include "types.h"

// V5: optional "Reset" button widget in the Blynk app.
BLYNK_WRITE(V5) {
  if (param.asInt() == 1 && g_processingTask) xTaskNotifyGive(g_processingTask);
}

void commsTask(void *) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);
  Blynk.config(BLYNK_AUTH_TOKEN);

  uint32_t lastWifiTryMs = millis();
  uint32_t lastBlynkTryMs = 0;
  uint32_t lastSendMs = 0;

  CommsMsg msg{};
  CommsMsg latest{};
  bool haveLatest = false;
  uint8_t pendingAlerts = 0;  // bitmask of AlarmReason values not yet delivered
  bool pendingReset = false;

  for (;;) {
    // Drain everything the processing task queued. Alerts and resets are remembered until
    // they are really delivered, so an offline period does not lose them.
    while (xQueueReceive(g_commsQueue, &msg, 0) == pdTRUE) {
      switch (msg.type) {
        case MsgType::Telemetry:
          latest = msg;
          haveLatest = true;
          break;
        case MsgType::Alert:
          pendingAlerts |= (uint8_t)(1u << msg.reason);
          break;
        case MsgType::SystemReset:
          pendingAlerts = 0;
          pendingReset = true;
          break;
      }
    }

    const uint32_t now = millis();

    if (WiFi.status() != WL_CONNECTED) {
      if (now - lastWifiTryMs >= WIFI_RETRY_MS) {
        lastWifiTryMs = now;
        WiFi.disconnect();
        WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);
        Serial.println("[comms] retrying WiFi");
      }
    } else if (!Blynk.connected()) {
      if (now - lastBlynkTryMs >= BLYNK_RETRY_MS) {
        lastBlynkTryMs = now;
        Serial.println(Blynk.connect(BLYNK_CONNECT_TIMEOUT_MS) ? "[comms] Blynk connected"
                                                                : "[comms] Blynk connect failed");
      }
    } else {
      Blynk.run();

      if (pendingReset) {
        Blynk.virtualWrite(V4, "System Reset");
        pendingReset = false;
      }
      for (int r = 1; r < REASON_COUNT; r++) {
        if (pendingAlerts & (1u << r)) {
          const char *text = reasonText((AlarmReason)r);
          Blynk.virtualWrite(V4, text);
          Blynk.logEvent(BLYNK_EVENT_NAME, text);  // push notification (event must exist in console)
          pendingAlerts &= (uint8_t)~(1u << r);
        }
      }
      if (haveLatest && now - lastSendMs >= TELEMETRY_PERIOD_MS) {
        lastSendMs = now;
        Blynk.virtualWrite(V0, latest.spo2);
        Blynk.virtualWrite(V1, latest.heartRate);
        Blynk.virtualWrite(V2, latest.accMagG);
        Blynk.virtualWrite(V3, latest.vibration);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(COMMS_LOOP_MS));
  }
}
