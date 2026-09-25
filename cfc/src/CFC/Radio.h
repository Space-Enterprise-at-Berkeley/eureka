#pragma once
#include <Arduino.h>
#include <Common.h>
#include <Comms.h>
#include <cstring>
#include <vector>
#include <map>

namespace RadioComms {
  void init();
  void processWaitingPackets();
  /**
   * @brief Sends packet data over ethernet and serial.
   *
   * @param packet The packet in which the data is stored.
   */
  void emitPacket(Comms::Packet *packet);
  uint32_t testRadioTransmit();
  uint32_t task_transmitCallsign();
  bool isRadioEnabled();
  void processWaitingPackets();
};