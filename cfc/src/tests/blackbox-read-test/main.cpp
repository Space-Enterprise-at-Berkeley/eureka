#include <Arduino.h>
#include <SPI.h>
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
  
  
  // blackbox setup
  BlackBox::init();
  while (BlackBox::busy()) {
      Serial.println("waiting for flash to be not busy...");
      delay(100);
  }

  // read data
  for (uint32_t i = 0; i < 32 * 900; i += 32) {
    BlackBox::getData(i, &packet);
    float hx = Comms::packetGetFloat(&packet, 0);
    float hy = Comms::packetGetFloat(&packet, 4);
    float hz = Comms::packetGetFloat(&packet, 8);
    float lx = Comms::packetGetFloat(&packet, 12);
    float ly = Comms::packetGetFloat(&packet, 16);
    float lz = Comms::packetGetFloat(&packet, 20);
    Serial.println("High IMU Readings:");
    Serial.print(" HX = ");
    Serial.println(hx);
    Serial.print(" HY = ");
    Serial.println(hy);
    Serial.print(" HZ = ");
    Serial.println(hz);
    Serial.println("Low IMU Readings:");
    Serial.print(" LX = ");
    Serial.println(lx);
    Serial.print(" LY = ");
    Serial.println(ly);
    Serial.print(" LZ = ");
    Serial.println(lz);
    Serial.println(" ");
  }
}

void loop() 
{
}