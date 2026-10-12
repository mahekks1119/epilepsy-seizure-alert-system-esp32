#include "detector.h"

void Detector::reset() {
  lowSpo2_ = {false, 0};
  highHr_ = {false, 0};
  for (int i = 0; i < JERK_WINDOW_RECORDS; i++) jerkWindow_[i] = false;
  jerkIdx_ = 0;
  jerkCount_ = 0;
  latched_ = false;
  reason_ = REASON_NONE;
}

bool Detector::sustained(Sustain &s, bool cond, uint32_t now, uint32_t holdMs) {
  if (!cond) {
    s.active = false;
    return false;
  }
  if (!s.active) {
    s.active = true;
    s.since = now;
    return false;
  }
  return (uint32_t)(now - s.since) >= holdMs;
}

AlarmReason Detector::update(const SensorRecord &r) {
  // Vitals are only judged when the PPG sensor is healthy and reporting; the motion path
  // keeps running regardless (the original code skipped everything on invalid PPG data).
  const bool vitalsValid = (r.status & SENSOR_PPG_OK) && r.heartRate > 0.0f && r.spo2 > 0.0f;
  const bool imuValid = (r.status & SENSOR_IMU_OK) != 0;

  const bool lowSpo2 = vitalsValid && r.spo2 < SPO2_ALARM_BELOW;
  const bool highHr = vitalsValid && r.heartRate > HR_ALARM_ABOVE;
  const bool jerk = imuValid && r.accDynPeakG > MOTION_THRESHOLD_G &&
                    (!REQUIRE_VIBRATION_FOR_MOTION || r.vibrationSeen);

  // Slide the jerk window.
  if (jerkWindow_[jerkIdx_]) jerkCount_--;
  jerkWindow_[jerkIdx_] = jerk;
  if (jerk) jerkCount_++;
  jerkIdx_ = (jerkIdx_ + 1) % JERK_WINDOW_RECORDS;

  const bool lowSustained = sustained(lowSpo2_, lowSpo2, r.tsMs, SPO2_SUSTAIN_MS);
  const bool hrSustained = sustained(highHr_, highHr, r.tsMs, HR_SUSTAIN_MS);
  const bool motion = jerkCount_ >= JERK_MIN_COUNT;

  if (latched_) return REASON_NONE;  // already alarming until reset

  AlarmReason raised = REASON_NONE;
  if (lowSustained) raised = REASON_LOW_SPO2;
  else if (hrSustained) raised = REASON_HIGH_HR;
  else if (motion) raised = REASON_SEIZURE_MOTION;

  if (raised != REASON_NONE) {
    latched_ = true;
    reason_ = raised;
  }
  return raised;
}
