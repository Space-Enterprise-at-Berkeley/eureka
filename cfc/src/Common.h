#pragma once

#include <Arduino.h>

#ifdef DEBUG_MODE
#define DEBUG(val) Serial.print(val)
#define DEBUG_FLUSH() Serial.flush()
#define DEBUGLN(val) Serial.println(val)
#else
#define DEBUG(val)
#define DEBUG_FLUSH()
#define DEBUGLN(val)
#endif

struct Task {
    uint32_t (*taskCall)(void);
    uint32_t nexttime;
    bool enabled;
};

/**
 * @brief vTaskDelayUntil that also records how late the task woke up, which is
 * reported as avgTaskDelay in the FC health packet.
 */
void taskDelayUntil(TickType_t *lastWake, TickType_t period);

#define initWire() Wire.setClock(400000); Wire.setPins(1,2); Wire.begin()

//Define Board ID Enum
enum BoardID
{
  AC1 = 11,
  AC2 = 12,
  AC3 = 13,
  LC1 = 21,
  LC2 = 22,
  PT1 = 31,
  PT2 = 32,
  PT3 = 33,
  PT_HAD = 34,
  TC1 = 51,
  TC2 = 52,
  FC = 42,
  NOS_CAP = 42,
  IPA_CAP = 43,
  IPA_EREG = 71,
  ALL = 255,
};