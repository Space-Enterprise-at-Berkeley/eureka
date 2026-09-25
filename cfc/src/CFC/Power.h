#pragma once

#include <Comms.h>
#include <INA233.h>

//reads power stats from INA233 and sends to ground station
namespace Power
{
  void init();
  void vTaskReadSendPower(void *pvParameters);
  void print();
}
