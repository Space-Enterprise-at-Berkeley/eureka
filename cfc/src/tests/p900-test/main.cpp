#include <Arduino.h>

/*
    * This is a test for communication between this and P900
    pass through data to USB serial monitor
*/

void setup() {
  // setup stuff here
  pinMode(10, OUTPUT);
  Serial1.begin(230400, SERIAL_8N1, 9, 10, false);
}

void loop() {
  Serial1.println("oimmm, tqwtm");
  delay(1000);
}