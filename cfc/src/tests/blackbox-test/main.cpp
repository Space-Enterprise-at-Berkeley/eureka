
#include <Arduino.h>
#include <SPIFlash.h>
#include "Blackbox.h"
#include <Comms.h>

Comms::Packet packet = {.id = 5};

void setup(){
    // Comms::init();

    delay(10000);

    Serial.begin(115200);
    Serial.println("Initializing....");
    pinMode(35, OUTPUT);
    pinMode(37, OUTPUT);
    pinMode(38, OUTPUT);
    pinMode(40, OUTPUT);
    digitalWrite(35, HIGH);
    digitalWrite(37, HIGH);
    digitalWrite(38, HIGH);
    digitalWrite(40, HIGH);
    BlackBox::init();

    Serial.println("booting");

    while (BlackBox::busy()) {
        Serial.println("waiting for flash to be not busy...");
        Serial.println("JEDEC ID: 0x" + String(BlackBox::getJEDECID(), HEX));
        delay(100);
    }

    /*
    BlackBox::startEraseAndRecord();

    while (BlackBox::busy()) {
        Serial.println("erasing...");
        delay(500);
    }
    Serial.println("erase complete");
    // #else
    // write 10 packets to blackbox
    for(uint32_t i = 0; i < 10; i++) {
        packet.len = 0;
        Comms::packetAddUint32(&packet, i);
        BlackBox::writePacket(&packet);
        Serial.println("hello, written a thing");
    }
    */
    
    BlackBox::playback();

    /*
    // read 1000 packets from blackbox and check if they have correct data

    for (uint32_t i = 0; i < 12 * 10; i += 12) {
        BlackBox::getData(i, &packet);
        uint32_t data = Comms::packetGetUint32(&packet, 0);
        Serial.printf("Blackbox packet %i has data %i \n", i/12, data);
    }
    // #endif
    */
}

void loop() {
    
}
