#include <Arduino.h>
#include <SPI.h>
#include <LSM6DSOSensor.h>
#define CS_PIN 40

// SPIClass dev_spi(15, 16, 17);  
// Create sensor instance with SPI
LSM6DSOSensor lms6d(&SPI, CS_PIN);


// Interrupts
volatile int mems_event = 0;
char report[256];

// Function prototypes
void INT1Event_cb();
void sendOrientation();

void setup() {
  Serial.begin(115200);
  // Initialize SPI bus
  pinMode(15, OUTPUT);    // MOSI for SPI
  pinMode(16, INPUT);     // MISO for SPI
  pinMode(17, OUTPUT);    // SCK for SPI
  SPI.begin(17,16,15);
  // Set CS pin as OUTPUT and set it HIGH (deselect)
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  // activate and check if working
  if (lms6d.begin()) {
      Serial.println("ERROR: LSM6DSO not detected, check wiring");
      return;
  }
  if (lms6d.Enable_X()) {
      Serial.println("ERROR: LSM6DSO accelerometer failed, check wiring");
      return;
  }
  if (lms6d.Enable_G()) {
      Serial.println("ERROR: LSM6DSO gyroscope failed, check wiring");
      return;
  }

  // Attach interrupt
  //  attachInterrupt(digitalPinToInterrupt(INT_1), INT1Event_cb, RISING);
}

void loop() {
   sendOrientation();
   delay(100);
}

// Interrupt Service Routine
void INT1Event_cb() {
  mems_event = 1;
}

// Function to send orientation data
void sendOrientation() {
  /*
  uint8_t xl = 0, xh = 0, yl = 0, yh = 0, zl = 0, zh = 0;

  lms6d.Get_6D_Orientation_XL(&xl);
  lms6d.Get_6D_Orientation_XH(&xh);
  lms6d.Get_6D_Orientation_YL(&yl);
  lms6d.Get_6D_Orientation_YH(&yh);
  lms6d.Get_6D_Orientation_ZL(&zl);
  lms6d.Get_6D_Orientation_ZH(&zh);
  
  if (xl == 0 && yl == 0 && zl == 0 && xh == 0 && yh == 1 && zh == 0) {
    sprintf(report, "Portrait Up");
  } else if (xl == 1 && yl == 0 && zl == 0) {
    sprintf(report, "Portrait Down");
  } else if (xh == 1 && yl == 0 && zl == 0) {
    sprintf(report, "Landscape Left");
  } else if (yl == 1 && xl == 0 && zl == 0) {
    sprintf(report, "Landscape Right");
  } else if (zh == 1) {
    sprintf(report, "Face Up");
  } else if (zl == 1) {
    sprintf(report, "Face Down");
  } else {
    sprintf(report, "Unknown Orientation");
  }

  Serial.print(report);
  */

  int32_t accelerometer[3];
  int32_t gyroscope[3];

  if (lms6d.Get_X_Axes(accelerometer)){
    Serial.println("Error reading accelerometer");
    return;
  }
  if (lms6d.Get_G_Axes(gyroscope)){
    Serial.println("Error reading gyroscope");
    return;
  }

  Serial.println(gyroscope[0]);
  Serial.println(gyroscope[1]);
  Serial.println(gyroscope[2]);
  Serial.println(accelerometer[0]);
  Serial.println(accelerometer[1]);
  Serial.println(accelerometer[2]);
  Serial.println("\n");
}
