#pragma once

#include <Common.h>
#include <Arduino.h>

#include <map>
#include <vector>
#include "proto/common.h"

#include <Comms.h>

namespace USBComms {

  uint32_t getBadPacketCount();
  void resetBadPacketCount();
  void init();
  void processWaitingPackets();
  /**
   * @brief Sends packet data over ethernet and serial.
   *
   * @param packet The packet in which the data is stored.
   */
  void emitPacket(Comms::Packet *packet);
};