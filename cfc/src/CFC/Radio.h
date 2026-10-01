#pragma once
#include <Arduino.h>
#include <Common.h>
#include <Comms.h>
#include <cstring>
#include <map>
#include <vector>

namespace RadioComms {
void init();
void processWaitingPackets();
/**
 * @brief Sends packet data over ethernet and serial.
 *
 * @param packet The packet in which the data is stored.
 */
void emitPacket(Comms::Packet *packet);
void vTaskTestRadioTransmit(void *pvParameters);
void vTaskTransmitCallsign(void *pvParameters);
bool isRadioEnabled();
bool isInConfigMode();
void processWaitingPackets();
}; // namespace RadioComms
