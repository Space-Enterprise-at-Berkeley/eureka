#pragma once

#include <Common.h>

#include <Arduino.h>

#include <map>
#include <vector>
#include "proto/common.h"

namespace Comms
{
    
//this is now abstracted from any communication interface

  struct Packet
  {
    uint8_t id;
    uint8_t len;
    uint8_t timestamp[4];
    uint8_t checksum[2];
    uint8_t data[256];
  };    

  typedef void (*commFunction)(Packet, uint8_t);

  /**
   * @brief Registers methods to be called when Comms receives a packet with a specific ID.
   *
   * @param id The ID of the packet associated with a specific command.
   * @param function a pointer to a method that takes in a Packet struct.
   */
  void registerCallback(uint8_t id, commFunction function);
  void evokeCallbackFunction(Packet *packet, uint8_t ip);

  void processWaitingPackets();

  void packetAddFloat(Packet *packet, float value);
  void packetAddUint32(Packet *packet, uint32_t value);
  void packetAddUint16(Packet *packet, uint16_t value);
  void packetAddUint8(Packet *packet, uint8_t value);

  /**
   * @brief Interprets the packet data as a float.
   *
   * @param packet
   * @param index The index of the byte array at which the float starts (0, 4, 8).
   * @return float
   */
  float packetGetFloat(Packet *packet, uint8_t index);
  uint32_t packetGetUint32(Packet *packet, uint8_t index);
  uint32_t packetGetUint8(Packet *packet, uint8_t index);

  /**
   * @brief Adds time and checksum to packet.
   *
   * @param packet Packet to be processed.
   */
  void finishPacket(Packet *packet);

  void emitPacketOverAllInterfaces(Packet *packet);

  bool verifyPacket(Packet *packet);

  uint16_t computePacketChecksum(Packet *packet);

  /**
   * @brief Broadcasts an abort packet with the current system mode and abort reason.
   *
   * @param systemMode The current system mode. Follows enum in Common.h.
   * @param abortReason The current abort reason. Follows enum in Common.h.
   */
  void sendAbort(uint8_t systemMode, uint8_t abortReason);
};
