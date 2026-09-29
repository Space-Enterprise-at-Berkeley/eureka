#include "TempSense.h"
#include "proto/Packet_FCTemperature.h"

// reads temp stats from LM75
namespace TempSense {
LM75 lm(0x4F);
float sendRate = 500 * 1000; // 0.5 second

Comms::Packet p;

void init() {
  // nothing needed
}

void vTaskReadSendTemp(void *pvParameters) {
  (void)pvParameters;
  Comms::Packet p;
  TickType_t lastWake = xTaskGetTickCount();
  while (1) {
    float tC = lm.temp();

    PacketFCTemperature::Builder().withTemp(tC).build().writeRawPacket(&p);
    Comms::emitPacketOverAllInterfaces(&p);

    taskDelayUntil(&lastWake, pdMS_TO_TICKS(500)); // .5 second
  }
}

void print() {
  float tC = lm.temp();
  Serial.print("Temp: ");
  Serial.print(tC);
  Serial.println(" °C");
}
} // namespace TempSense
