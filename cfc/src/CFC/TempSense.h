#pragma once

#include <Arduino.h>
#include <LM75.h>
#include <Comms.h>


namespace TempSense
{
  void init();
  uint32_t task_readSendTemp();
  void print();
}
