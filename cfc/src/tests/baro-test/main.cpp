/*
    * This is a test for reading from the MS560702BA03-50 Barometer
    TODO: Implement this test
*/
#include <Arduino.h>
#include <SPI.h>
#include <MS5607.h>
#define CS_PIN 37
MS5607 P_Sens = MS5607(CS_PIN);

void setup(void) {
   Serial.begin(115200);
   pinMode(CS_PIN, OUTPUT);
   digitalWrite(CS_PIN, HIGH);
   P_Sens.begin();
   Serial.println("MS5607 initialized");
}

void loop(void) {
   float T_val, P_val, H_val;
   P_Sens.readDigitalValue();
   // print raw values
   Serial.print("DT: "); Serial.println(P_Sens.DT);
   Serial.print("DP: "); Serial.println(P_Sens.DP);
   Serial.print("C1: "); Serial.println(P_Sens.C1);
   Serial.print("C2: "); Serial.println(P_Sens.C2);
   Serial.print("C3: "); Serial.println(P_Sens.C3);
   Serial.print("C4: "); Serial.println(P_Sens.C4);
   Serial.print("C5: "); Serial.println(P_Sens.C5);
   Serial.print("C6: "); Serial.println(P_Sens.C6);
   T_val = P_Sens.getTemperature();
   P_val = P_Sens.getPressure();
   H_val = P_Sens.getAltitude();
   Serial.print("Temperature :  ");
   Serial.print(T_val);
   Serial.println(" C");
   Serial.print("Pressure    :  ");
   Serial.print(P_val);
   Serial.println(" mBar");
   Serial.print("Altitude    :  ");
   Serial.print(H_val);
   Serial.println(" meter");
   delay(1000);
}
