#pragma once

#include "Comms.h"
#include "proto/Packet_AvgBaroValues.h"
#include "proto/Packet_BaroValues.h"
#include "proto/Packet_FCEnableBaroCalibration.h"
#include <MS5607.h>

namespace Barometer {
extern float altitude, pressure, temperature;
void init(void);
extern bool calibrated;
void vTaskSampleBaro(void *pvParameters);
void vTaskAverageBaro(void *pvParameters);
void en_baro_callback(Comms::Packet packet, uint8_t ip);
} // namespace Barometer
