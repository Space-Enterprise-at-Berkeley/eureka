#pragma once

#include <Arduino.h>
#include <SPIFlash.h>
#include "Comms.h"
#include "USBComms.h"
#include "Radio.h"

namespace Blackbox {
    const uint32_t FLASH_SIZE = 1.6e7;
    extern bool erasing;
    const uint8_t cs = 39;

    void init();
    void writePacket(Comms::Packet *packet);
    bool getData(uint32_t byteAddress, Comms::Packet* packet);
    Comms::Packet getData(uint32_t byteAddress);
    void startEraseAndRecord();
    void packetHandler(Comms::Packet packet, uint8_t _);
    void playback();

    uint32_t getAddr();
    bool getEnable();
    uint32_t reportStoragePacket();
    bool busy();
    uint16_t getJEDECID();
}