#pragma once

#include "Comms.h"
#include "proto/Packet_BaroValues.h"
#include "proto/Packet_AvgBaroValues.h"
#include "proto/Packet_FCEnableBaroCalibration.h"
#include <MS5607.h>
                             
namespace Barometer {
    extern float altitude, pressure, temperature;
    void init(void);
    extern bool calibrated;
    uint32_t sampleBaro(void);
    uint32_t averageBaro(void);
    void en_baro_callback(Comms::Packet packet, uint8_t ip);
    uint32_t zeroAltitude(void);
}