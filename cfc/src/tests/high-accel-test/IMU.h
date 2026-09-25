#pragma once

#include <Arduino.h>
#include <Comms.h>
#include <SPI.h>

#define LOW_IMU_CS_PIN 40
#define HIGH_IMU_CS_PIN 38
#define HIGH_IMU_X_OFFSET -0.65f
#define HIGH_IMU_Y_OFFSET -0.9f
#define HIGH_IMU_Z_OFFSET -1.88f
#define HIGH_IMU_X_MULTIPLIER 0.29f
#define HIGH_IMU_Y_MULTIPLIER 0.29f
#define HIGH_IMU_Z_MULTIPLIER 0.25f

// reads low and high IMU readings and sends to ground station
namespace IMU {
  void init_lowIMU();
  uint32_t task_lowIMUsend();
  void getLowIMU(float *readings);

  void init_highIMU();
  uint32_t task_highIMUsend();
  void getHighIMU(float *readings);
}