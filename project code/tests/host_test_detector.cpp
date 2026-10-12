// Host-side unit tests for the detection logic. No hardware needed.
// Build and run:  g++ -std=c++11 -Wall -I.. ../detector.cpp host_test_detector.cpp -o host_test && ./host_test
#include <cstdio>
#include "detector.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static SensorRecord mk(uint32_t t, float hr, float spo2, float dyn, uint8_t vib,
                       uint8_t status = SENSOR_PPG_OK | SENSOR_IMU_OK) {
  SensorRecord r{};
  r.tsMs = t; r.heartRate = hr; r.spo2 = spo2; r.accMagG = 1.0f;
  r.accDynPeakG = dyn; r.vibrationSeen = vib; r.status = status;
  return r;
}

// Feed 'seconds' of 100 ms records; returns the time of the first alarm or 0.
static uint32_t run(Detector &d, uint32_t t0, int seconds, float hr, float spo2, float dyn, uint8_t vib,
                    uint8_t status = SENSOR_PPG_OK | SENSOR_IMU_OK) {
  for (int i = 0; i < seconds * 10; i++) {
    uint32_t t = t0 + i * 100;
    if (d.update(mk(t, hr, spo2, dyn, vib, status)) != REASON_NONE) return t;
  }
  return 0;
}

int main() {
  { Detector d;  // normal vitals and rest for 30 s
    CHECK(run(d, 1000, 30, 75, 98, 0.05f, 0) == 0); CHECK(!d.alarmLatched()); }

  { Detector d;  // sustained low SpO2 alarms after ~5 s
    uint32_t t = run(d, 1000, 10, 75, 85, 0.05f, 0);
    CHECK(t >= 6000 && t <= 6200); CHECK(d.reason() == REASON_LOW_SPO2); }

  { Detector d;  // a single low SpO2 sample is ignored
    CHECK(run(d, 1000, 3, 75, 98, 0.05f, 0) == 0);
    CHECK(d.update(mk(4000, 75, 80, 0.05f, 0)) == REASON_NONE);
    CHECK(run(d, 4100, 10, 75, 98, 0.05f, 0) == 0); }

  { Detector d;  // sustained high heart rate
    uint32_t t = run(d, 1000, 10, 135, 97, 0.05f, 0);
    CHECK(t != 0); CHECK(d.reason() == REASON_HIGH_HR); }

  { Detector d;  // rhythmic jerks with vibration -> seizure-like motion
    uint32_t t = run(d, 1000, 5, 75, 97, 2.5f, 1);
    CHECK(t != 0); CHECK(d.reason() == REASON_SEIZURE_MOTION); }

  { Detector d;  // jerks without vibration -> no alarm (as the original required both)
    CHECK(run(d, 1000, 10, 75, 97, 2.5f, 0) == 0); }

  { Detector d;  // a 5-record bump (0.5 s) -> no alarm
    CHECK(run(d, 1000, 2, 75, 97, 0.05f, 0) == 0);
    for (int i = 0; i < 5; i++) CHECK(d.update(mk(3000 + i * 100, 75, 97, 2.5f, 1)) == REASON_NONE);
    CHECK(run(d, 3500, 10, 75, 97, 0.05f, 0) == 0); }

  { Detector d;  // latch, then reset clears everything
    CHECK(run(d, 1000, 5, 75, 97, 2.5f, 1) != 0);
    CHECK(d.alarmLatched());
    CHECK(run(d, 6000, 5, 75, 97, 2.5f, 1) == 0);  // no repeat alarms while latched
    d.reset(); CHECK(!d.alarmLatched());
    CHECK(run(d, 12000, 10, 75, 97, 0.05f, 0) == 0); }

  { Detector d;  // PPG unavailable: vitals ignored, motion still alarms
    CHECK(run(d, 1000, 10, 0, 0, 0.05f, 0, SENSOR_IMU_OK) == 0);
    CHECK(run(d, 11000, 5, 0, 0, 2.5f, 1, SENSOR_IMU_OK) != 0); }

  { Detector d;  // IMU unavailable: no motion alarms
    CHECK(run(d, 1000, 10, 75, 97, 2.5f, 1, SENSOR_PPG_OK) == 0); }

  if (fails == 0) std::printf("All detector tests passed\n");
  return fails ? 1 : 0;
}
