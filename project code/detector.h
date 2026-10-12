#pragma once
// Pure detection logic: no Arduino or FreeRTOS calls, so it can be unit-tested on a PC.
#include <stdint.h>
#include "config.h"
#include "types.h"

class Detector {
 public:
  Detector() { reset(); }
  void reset();
  // Feed one record. Returns the reason if an alarm was raised by this record, else REASON_NONE.
  AlarmReason update(const SensorRecord &r);
  bool alarmLatched() const { return latched_; }
  AlarmReason reason() const { return reason_; }

 private:
  struct Sustain {
    bool active;
    uint32_t since;
  };
  static bool sustained(Sustain &s, bool cond, uint32_t now, uint32_t holdMs);

  Sustain lowSpo2_;
  Sustain highHr_;
  bool jerkWindow_[JERK_WINDOW_RECORDS];
  int jerkIdx_;
  int jerkCount_;
  bool latched_;
  AlarmReason reason_;
};
