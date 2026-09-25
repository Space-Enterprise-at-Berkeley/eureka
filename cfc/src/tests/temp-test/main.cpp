
#include <Arduino.h>
#include <Wire.h>

#include <LM75.h>

#define LM75_ADDR 0x4F

LM75 lm(LM75_ADDR);

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(1, 2);         
  Wire.setClock(400000);
}

void loop() {
  float tC = lm.temp();
  Serial.print("Temp: ");
  Serial.print(tC);
  Serial.println(" °C");
  delay(500);
}