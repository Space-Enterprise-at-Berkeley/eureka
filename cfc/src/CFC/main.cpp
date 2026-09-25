#include <Arduino.h>
#include "Common.h"
#include <Comms.h>
#include <USBComms.h>
#include "Power.h"
#include "Barometer.h"
#include "TempSense.h"
#include "Blackbox.h"
#include "Radio.h"
#include <Wire.h>
#include <SPI.h>
#include "proto/Packet_DummyData1s.h"
#include "proto/Packet_DummyData100ms.h"
#include "proto/Packet_DummyData10ms.h"
#include "proto/Packet_DummyData1ms.h"
#include "proto/Packet_FCEnableRuncamPDB.h"
#include "proto/Packet_FCHealth.h"
#include "Radio.h"
#include "IMU.h"
#include "GPS.h"

#define LED_PIN 41
#define PDB_ENABLE_PIN 26

bool pinstate = 0;
uint32_t task_helloWorld() {
  Serial.println("my name sohom roy");
  digitalWrite(LED_PIN, pinstate);
  pinstate = !pinstate;
  return 500 * 1000; // this task will run 500ms
}

volatile bool runcam_PDB_enabled = false;
void runcam_PDB_enable_cb(Comms::Packet packet, uint8_t ip) {
  Serial.println("Received PDB enable command");
  if (Comms::packetGetUint8(&packet, 0)) {
    digitalWrite(PDB_ENABLE_PIN, HIGH);
    runcam_PDB_enabled = true;
    Serial.println("PDB Enabled");
    Serial.print("Runcam: ");
    Serial.println(runcam_PDB_enabled ? 1 : 0);
  } else {
    digitalWrite(PDB_ENABLE_PIN, LOW);
    runcam_PDB_enabled = false;
    Serial.println("PDB Disabled");
  }
}

uint32_t avgTaskDelay = 0;
uint32_t avgTaskCounter = 0;
Comms::Packet p;
uint32_t task_sendCFCHealth() {
  Serial.print("Runcam: ");
  Serial.println(runcam_PDB_enabled ? 1 : 0);
  PacketFCHealth::Builder()
    .withRuncamEnabled(runcam_PDB_enabled ? 1 : 0)
    .withAvgTaskDelay(avgTaskCounter == 0 ? 0 : avgTaskDelay / avgTaskCounter)
    .withBlackboxWritePointer(Blackbox::getAddr())
    .withRadioEnabled(RadioComms::isRadioEnabled() ? 1 : 0)
    .withBlackboxEnabled(Blackbox::getEnable() ? 1 : 0)
    .build()
    .writeRawPacket(&p);
  Comms::emitPacketOverAllInterfaces(&p);
  avgTaskDelay = 0;
  avgTaskCounter = 0;
  return 1000*1000; // this task will run every second
}


Task taskTable[] = {
  {task_helloWorld, 0, true},
  {Power::task_readSendPower, 0, true},
  {Barometer::sampleBaro, 0, true},
  {Barometer::averageBaro,0, true},
  {RadioComms::task_transmitCallsign, 0, true},
  {TempSense::task_readSendTemp, 0, true},
  {IMU::task_lowIMUsend, 0, true},
  {IMU::task_highIMUsend, 0, true},
  {GPS::task_readGPS, 0, true},
  {GPS::task_readGPSExtra, 0, true},
  {GPS::task_readGPSSatInfo, 0, true},
  {task_sendCFCHealth, 0, true},
};

#define TASK_COUNT (sizeof(taskTable) / sizeof (struct Task))

void setup() {
  // setup stuff here
  pinMode(17, OUTPUT);    // SCK for SPI
  pinMode(16, INPUT);     // MISO for SPI
  pinMode(15, OUTPUT);    // MOSI for SPI
  SPI.begin(17, 16, 15);
  Wire.begin(1, 2);
  USBComms::init();
  Serial.println("bee"); // THIS IS A LOAD-BEARING BEE, DO NOT REMOVE
  Serial.println("bee 2");
  // Serial.println("bee 3");
  RadioComms::init();
  Power::init();
  Barometer::init();
  TempSense::init();
  pinMode(LED_PIN, OUTPUT);
  pinMode(41, OUTPUT);
  IMU::init_lowIMU();
  IMU::init_highIMU();
  Comms::registerCallback(PACKET_ID_FCEnableRuncamPDB, runcam_PDB_enable_cb);
  pinMode(PDB_ENABLE_PIN, OUTPUT);
  digitalWrite(PDB_ENABLE_PIN, LOW); // ensure PDB is off at startup
  runcam_PDB_enabled = false;
  GPS::init();
  Blackbox::init();

  while(1) {
    // main loop here to avoid arduino overhead
    for(uint32_t i = 0; i < TASK_COUNT; i++) { // for each task, execute if next time >= current time
      uint32_t ticks = micros(); // current time in microseconds
      uint32_t diff = taskTable[i].nexttime - ticks;
      if (diff > UINT32_MAX / 2 && taskTable[i].enabled) {
        avgTaskDelay += -diff;
        avgTaskCounter++;
        uint32_t delayoftask = taskTable[i].taskCall();
        if (delayoftask == 0) {
          taskTable[i].enabled = false;
        }
        else {
          taskTable[i].nexttime = ticks + delayoftask;
        }
      }
    }
    Comms::processWaitingPackets();
  }
}

void loop() {

} // unused