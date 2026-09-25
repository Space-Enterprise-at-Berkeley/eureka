#pragma once

#include "Comms.h"
#include <LM75.h>

namespace TempSense {
void init();
void vTaskReadSendTemp(void *pvParameters);
void print();
} // namespace TempSense
