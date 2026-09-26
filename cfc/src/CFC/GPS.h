#pragma once

#include <cstdint>

namespace GPS {
void init();
void vTaskReadGPS(void *pvParameters);
void vTaskReadGPSExtra(void *pvParameters);
void vTaskReadGPSSatInfo(void *pvParameters);
} // namespace GPS
