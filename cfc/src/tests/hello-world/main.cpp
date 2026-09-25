
#include <Arduino.h>
#include <SPIFlash.h>

void setup() {
  Serial.begin(115200);
}

void loop() {
    Serial.println("hello World");
    delay(500);

    //blink LED or something
} // unused
