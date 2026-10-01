#include "Power.h"
#include "proto/Packet_24VSupplyStats.h"

// reads power stats from INA233 and sends to ground station
namespace Power {
INA233 ina(INA233_ADDRESS_41, Wire);
float rShunt = 0.004;
float iMax = 5.0;
float sendRate = 1000 * 1000; // 0.5 second

Comms::Packet p;

void init() { ina.init(rShunt, iMax); }

void vTaskReadSendPower(void *pvParameters) {
  (void)pvParameters;
  TickType_t lastWake = xTaskGetTickCount();
  while (1) {
    // read the ina
    float busVoltage = ina.readBusVoltage();
    float shuntCurrent = ina.readCurrent();
    float power = ina.readPower();

    // make Packet
    Packet24VSupplyStats::Builder()
        .withSupply24Voltage(busVoltage)
        .withSupply24Current(shuntCurrent)
        .withSupply24Power(power)
        .build()
        .writeRawPacket(&p);

    // emit the packet
    Comms::emitPacketOverAllInterfaces(&p);

    taskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));
  }
}

void print() {
  // read the ina
  float busVoltage = ina.readBusVoltage();
  float shuntCurrent = ina.readCurrent();
  // float shuntVoltage = ina.readShuntVoltage(); don't need this
  float power = ina.readPower();
  // float avgPower = ina.readAvgPower(); eh maybe?

  // print the ina
  Serial.print("Bus Voltage: ");
  Serial.println(busVoltage);
  Serial.print("Shunt Current: ");
  Serial.println(shuntCurrent);
  Serial.print("Power: ");
  Serial.println(power);
}
} // namespace Power
