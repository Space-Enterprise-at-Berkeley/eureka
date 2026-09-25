#include <Arduino.h>
#include <Comms.h>
#include <USBComms.h>

/*
    * This is a test for writing from CFC
    TODO: Implement this test
*/


//[[!  ~ $C4AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA]]
// typeable packet ^

bool pinState = false;
void led_callback(Comms::Packet packet, uint8_t ip) {
  uint8_t idx = 0;
  Serial.println("Got packet!");
  digitalWrite(41, pinState);
  pinState = !pinState;
}

void setup() {
  // setup stuff here
  uint8_t id = 33;
  USBComms::init();
  pinMode(41, OUTPUT);
  Comms::registerCallback(id, led_callback);
}


void loop() {
  Comms::processWaitingPackets();
  //Serial.println(USBComms::getBadPacketCount());
}