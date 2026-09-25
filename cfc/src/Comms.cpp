#include <Comms.h>
#include <USBComms.h>
#include "CFC/Radio.h"
#include "CFC/Blackbox.h"
//#include RemoteBB.h
// Or something like this

#include "proto/Packet_Abort.h" // This can't go in the header or it will cause a circular import of headers

namespace Comms {

  bool showPacketRecv = true;
 
  void processWaitingPackets()
  {
    USBComms::processWaitingPackets();
    RadioComms::processWaitingPackets();
    // no incoming packets from anything else
  }

  void emitPacketOverAllInterfaces(Packet *packet)
  {
    finishPacket(packet);

    //Call respective send functions for everyone
    USBComms::emitPacket(packet);
    RadioComms::emitPacket(packet);
    Blackbox::writePacket(packet);
    //RemoteBlackbox::writePacket(packet);
  }

  std::map<uint8_t, commFunction> callbackMap;

  void registerCallback(uint8_t id, commFunction function)
  {
    callbackMap.insert(std::pair<int, commFunction>(id, function));
  }

    /**
   * @brief Checks checksum of packet and tries to call the associated callback function.
   *
   * @param packet Packet to be processed.
   * @param ip End byte in IP address of sender (255 if usb packet)
   */
  void evokeCallbackFunction(Packet *packet, uint8_t ip)
  {
    uint16_t checksum = *(uint16_t *)&packet->checksum;
    if (checksum == computePacketChecksum(packet))
    {
      if (showPacketRecv)
      {
        Serial.print("Packet with ID ");
        Serial.print(packet->id);
        Serial.print(" has correct checksum!\n");
      }
      // try to access function, checking for out of range exception
      if (callbackMap.count(packet->id))
      {
        callbackMap.at(packet->id)(*packet, ip);
      }
      else
      {
        if (showPacketRecv)
        {
          Serial.print("ID ");
          Serial.print(packet->id);
          Serial.print(" does not have a registered callback function.\n");
        }
      }
    } else {
      Serial.print("Packet with ID ");
      Serial.print(packet->id);
      Serial.print(" has incorrect checksum!\n");
    }
  }


  void packetAddFloatArray(Packet *packet, float *arr, uint8_t len)
  {
    for (int i = 0; i < len; i ++){
      packetAddFloat(packet, *(arr + i));
    }
  }

  void packetAddFloat(Packet *packet, float value)
  {
    uint32_t rawData = *(uint32_t *)&value;
    packet->data[packet->len] = rawData & 0xFF;
    packet->data[packet->len + 1] = rawData >> 8 & 0xFF;
    packet->data[packet->len + 2] = rawData >> 16 & 0xFF;
    packet->data[packet->len + 3] = rawData >> 24 & 0xFF;
    packet->len += 4;
  }

  void packetAddUint32(Packet *packet, uint32_t value)
  {
    packet->data[packet->len] = value & 0xFF;
    packet->data[packet->len + 1] = value >> 8 & 0xFF;
    packet->data[packet->len + 2] = value >> 16 & 0xFF;
    packet->data[packet->len + 3] = value >> 24 & 0xFF;
    packet->len += 4;
  }

  void packetAddUint16(Packet *packet, uint16_t value)
  {
    packet->data[packet->len] = value & 0xFF;
    packet->data[packet->len + 1] = value >> 8 & 0xFF;
    packet->len += 2;
  }

  void packetAddUint8(Packet *packet, uint8_t value)
  {
    packet->data[packet->len] = value;
    packet->len++;
  }

  float packetGetFloat(Packet *packet, uint8_t index)
  {
    uint32_t rawData = packet->data[index + 3];
    rawData <<= 8;
    rawData += packet->data[index + 2];
    rawData <<= 8;
    rawData += packet->data[index + 1];
    rawData <<= 8;
    rawData += packet->data[index];
    return *(float *)&rawData;
  }

  uint32_t packetGetUint32(Packet *packet, uint8_t index)
  {
    uint32_t rawData = packet->data[index + 3];
    rawData <<= 8;
    rawData += packet->data[index + 2];
    rawData <<= 8;
    rawData += packet->data[index + 1];
    rawData <<= 8;
    rawData += packet->data[index];
    return rawData;
  }

  uint32_t packetGetUint8(Packet *packet, uint8_t index)
  {
    return packet->data[index];
  }

  /**
   * @brief Sends packet to both groundstations.
   *
   * @param packet Packet to be sent.
   */

  void finishPacket(Packet *packet){
    // add timestamp to struct
    uint32_t timestamp = millis();
    packet->timestamp[0] = timestamp & 0xFF;
    packet->timestamp[1] = (timestamp >> 8) & 0xFF;
    packet->timestamp[2] = (timestamp >> 16) & 0xFF;
    packet->timestamp[3] = (timestamp >> 24) & 0xFF;

    // calculate and append checksum to struct
    uint16_t checksum = computePacketChecksum(packet);
    packet->checksum[0] = checksum & 0xFF;
    packet->checksum[1] = checksum >> 8;

  }

  bool verifyPacket(Packet *packet)
  {
    uint16_t csum = computePacketChecksum(packet);
    return ((uint8_t)csum & 0xFF) == packet->checksum[0] && ((uint8_t)(csum >> 8)) == packet->checksum[1];
  }

  /**
   * @brief generates a 2 byte checksum from the information of a packet
   *
   * @param data pointer to data array
   * @param len length of data array
   * @return uint16_t
   */
  uint16_t computePacketChecksum(Packet *packet)
  {

    uint8_t sum1 = 0;
    uint8_t sum2 = 0;

    sum1 = sum1 + packet->id;
    sum2 = sum2 + sum1;
    sum1 = sum1 + packet->len;
    sum2 = sum2 + sum1;

    for (uint8_t index = 0; index < 4; index++)
    {
      sum1 = sum1 + packet->timestamp[index];
      sum2 = sum2 + sum1;
    }

    for (uint8_t index = 0; index < packet->len; index++)
    {
      sum1 = sum1 + packet->data[index];
      sum2 = sum2 + sum1;
    }
    return (((uint16_t)sum2) << 8) | (uint16_t)sum1;
  }

  void sendAbort(uint8_t systemMode, uint8_t abortReason){
    Packet packet;
    PacketAbort::Builder()
      .withSystemMode((SystemMode) systemMode)
      .withAbortReason((AbortCode) abortReason)
      .build()
      .writeRawPacket(&packet);
    emitPacketOverAllInterfaces(&packet);
    Serial.println("Abort sent, mode " + String((SystemMode)systemMode) + " reason " + String((AbortCode)abortReason));
  }
};