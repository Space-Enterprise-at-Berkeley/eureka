#include <Arduino.h>
#include <SPI.h>
#include "IMU.h"
#include "Blackbox.h"

Comms::Packet packet = {.id = 5};

void setup() 
{
  delay(10000);
  Serial.begin(115200);
  pinMode(17, OUTPUT);    // SCK for SPI
  pinMode(16, INPUT);     // MISO for SPI
  pinMode(15, OUTPUT);    // MOSI for SPI
  SPI.begin(17, 16, 15);
  pinMode(35, OUTPUT);
  pinMode(37, OUTPUT);
  pinMode(38, OUTPUT);
  pinMode(40, OUTPUT);
  digitalWrite(35, HIGH);
  digitalWrite(37, HIGH);
  digitalWrite(38, HIGH);
  digitalWrite(40, HIGH);
  
  while (Serial.read() != 't') {
    Serial.println("Press 't' to start test...");
    delay(100);
  }
  
  // blackbox setup
  BlackBox::init();
  while (BlackBox::busy()) {
      Serial.println("waiting for flash to be not busy...");
      delay(100);
  }
  BlackBox::startEraseAndRecord();
  while (BlackBox::busy()) {
      Serial.println("erasing...");
      delay(500);
  }
  Serial.println("erase complete");

  // high accel IMU setup
  IMU::init_highIMU();
}

void loop() 
{
  float readingsH[3];
  float readingsL[3];
  IMU::getHighIMU(readingsH);
  IMU::getLowIMU(readingsL);
  Serial.println("High IMU Readings:");
  Serial.print(" X = ");
  Serial.println(readingsH[0], 3);
  Serial.print(" Y = ");
  Serial.println(readingsH[1], 3);
  Serial.print(" Z = ");
  Serial.println(readingsH[2], 3);
  Serial.println("Low IMU Readings:");
  Serial.print(" X = ");
  Serial.println(readingsL[0], 3);
  Serial.print(" Y = ");
  Serial.println(readingsL[1], 3);
  Serial.print(" Z = ");
  Serial.println(readingsL[2], 3);
  Serial.println(" ");
  packet.len = 0;
  Comms::packetAddFloat(&packet, readingsH[0]);
  Comms::packetAddFloat(&packet, readingsH[1]);
  Comms::packetAddFloat(&packet, readingsH[2]);
  Comms::packetAddFloat(&packet, readingsL[0]);
  Comms::packetAddFloat(&packet, readingsL[1]);
  Comms::packetAddFloat(&packet, readingsL[2]);
  BlackBox::writePacket(&packet);
  delay(200);
}