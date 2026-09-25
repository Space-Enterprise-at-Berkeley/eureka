#include "Blackbox.h"
#include "proto/Packet_FCEnableBB.h"

namespace Blackbox {
    uint16_t expectedDeviceID = 0xEF40;
    SPIFlash flash(cs, expectedDeviceID, 17, 16, 15);
    uint32_t addr;
    bool erasing = false;
    bool enable = false;

    void init() {
        pinMode(cs, OUTPUT);
        if (flash.initialize()) {
            Serial.println("Init OK!");
        } else {
            Serial.print("Init FAIL, expectedDeviceID(0x");
            Serial.print(expectedDeviceID, HEX);
            Serial.print(") mismatched the read value: 0x");
            Serial.println(flash.readDeviceId(), HEX);
        }
        Comms::registerCallback(PACKET_ID_FCEnableBB, packetHandler);
    }
    
    void packetHandler(Comms::Packet packet, uint8_t _) {
        if (Comms::packetGetUint8(&packet, 0) == 1) {
            Serial.println("Received erase and start recording command");
            startEraseAndRecord();
        } else {
            Serial.println("Received stop recording command");
            enable = false;
            erasing = false;
        }
    }

    uint16_t getJEDECID() {
        return flash.readDeviceId();
    }

    bool busy() {
        return flash.busy();
    }

    void writePacket(Comms::Packet *packet) {
        if (erasing && flash.busy()) {
            Serial.println("still erasing!");
            return;
        }
        //Serial.printf("busy: %d, erasing: %d, addr: %d, enable: %d, writing\n", flash.busy(), erasing, addr, enable);
        if (enable) {
            uint16_t len = 8 + packet->len;
            flash.writeBytes(addr, packet, len);
            addr += len;
            if (addr > (FLASH_SIZE * 0.99)) {
                enable = false;
            }
        }
    }

    bool getData(uint32_t byteAddress, Comms::Packet* packet) {
        flash.readBytes(byteAddress, packet, sizeof(Comms::Packet));
        return Comms::verifyPacket(packet);
    }

    Comms::Packet getData(uint32_t byteAddress) {
        Comms::Packet packet;
        flash.readBytes(byteAddress, &packet, sizeof(Comms::Packet));
        return packet;
    }

    void startEraseAndRecord() {
        if (erasing || enable) {
            Serial.println("already done!");
            return;
        }
        Serial.println("starting chip erase");
        flash.chipErase();
        erasing = true;
        enable = true;
        addr = 0;
    }

    void getAllData() {
        for(int i = 0; i < addr; i++) {
            Serial.write(flash.readByte(addr));
        }
    }
    
    void playback() {
        uint32_t curr = 0;
        while (curr < FLASH_SIZE) {
            // read packet size and data
            Comms::Packet packet;
            flash.readBytes(curr, &packet, sizeof(Comms::Packet));
            uint16_t len = 8 + packet.len;
            flash.readBytes(curr, &packet, len);
            
            // check that packet is valid
            if (packet.len == 0xFF) {
                break;
            }
            
            // emit packet over interfaces
            USBComms::emitPacket(&packet);
            RadioComms::emitPacket(&packet);
            curr += len;
        }
    }

    uint32_t getAddr() {
        return addr;
    }

    bool getEnable() {
        return enable;
    }

    //Comms::Packet sizePacket = {.id = 12};

    // uint32_t reportStoragePacket() { 
    //     sizePacket.len = 0;
    //     Comms::packetAddUint32(&sizePacket, (getAddr() / 1000) + (erasing ? 1 : 0));
    //     Comms::emitPacketOverAllInterfaces(&sizePacket);
    //     // Radio::forwardPacket(&sizePacket);
    //     // writePacket(&sizePacket);
    //     Serial.println("Reported black box packet");
    //     return 500*1000;
    // }
}