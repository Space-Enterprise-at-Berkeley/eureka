#include <Arduino.h>
#include <SPI.h>
#include <LSM6DSOSensor.h>
#include <LIS331.h>
#include <vector>
#include <numeric>
#include "IMU.h"

namespace IMU {
  float_t quaterionViaLow[4];

  LSM6DSOSensor lms6d(&SPI, LOW_IMU_CS_PIN);
  LIS331 h3lis;
  
  std::vector<float> accelXBuffer;
  std::vector<float> accelYBuffer;
  std::vector<float> accelZBuffer;

  void init_lowIMU() {
    pinMode(LOW_IMU_CS_PIN, OUTPUT); // CS for SPI
    digitalWrite(LOW_IMU_CS_PIN, HIGH); // Make CS high
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
    lms6d.Set_X_FS(16);
  }

  void init_highIMU() {
    pinMode(HIGH_IMU_CS_PIN, OUTPUT); // CS for SPI
    digitalWrite(HIGH_IMU_CS_PIN, HIGH); // Make CS high
    h3lis.setSPICSPin(HIGH_IMU_CS_PIN);
    h3lis.axesEnable(true);
    // h3lis.setPowerMode(LIS331::NORMAL);
    // h3lis.setODR(LIS331::DR_1000HZ);
    h3lis.setHighPassCoeff(LIS331::HPC_64);
    h3lis.setFullScale(LIS331::HIGH_RANGE);
    h3lis.enableHPF(true);
    h3lis.begin(LIS331::USE_SPI);
  }
  
  // convert raw accelerometer readings to g's
  float convertAccel(int16_t raw, int32_t fullScale) {
    float sens_mg = LSM6DSO_ACC_SENSITIVITY_FS_2G;
    switch (fullScale) {
      case 2:  sens_mg = LSM6DSO_ACC_SENSITIVITY_FS_2G;  break;
      case 4:  sens_mg = LSM6DSO_ACC_SENSITIVITY_FS_4G;  break;
      case 8:  sens_mg = LSM6DSO_ACC_SENSITIVITY_FS_8G;  break;
      case 16: sens_mg = LSM6DSO_ACC_SENSITIVITY_FS_16G; break;
      default: sens_mg = LSM6DSO_ACC_SENSITIVITY_FS_2G;  break;
    }
    return (raw * 2 * sens_mg) / 1000.0;
  }
  
  // convert raw gyroscope readings to dps
  float convertGyro(int16_t raw, int32_t fullScale) {
    float sens_dps = LSM6DSO_GYRO_SENSITIVITY_FS_250DPS;
    switch (fullScale) {
      case 125:   sens_dps = LSM6DSO_GYRO_SENSITIVITY_FS_125DPS;   break;
      case 250:   sens_dps = LSM6DSO_GYRO_SENSITIVITY_FS_250DPS;   break;
      case 500:   sens_dps = LSM6DSO_GYRO_SENSITIVITY_FS_500DPS;   break;
      case 1000:  sens_dps = LSM6DSO_GYRO_SENSITIVITY_FS_1000DPS;  break;
      case 2000:  sens_dps = LSM6DSO_GYRO_SENSITIVITY_FS_2000DPS;  break;
      default:    sens_dps = LSM6DSO_GYRO_SENSITIVITY_FS_250DPS;   break;
    }
    return (raw * sens_dps) / 1000.0;
  }
  
  void imuAverages() {
    int16_t x, y, z;
    h3lis.readAxes(x, y, z);

    accelXBuffer.push_back(HIGH_IMU_X_MULTIPLIER * h3lis.convertToG(400,x) + HIGH_IMU_X_OFFSET);
    accelYBuffer.push_back(HIGH_IMU_Y_MULTIPLIER * h3lis.convertToG(400,y) + HIGH_IMU_Y_OFFSET);
    accelZBuffer.push_back(HIGH_IMU_Z_MULTIPLIER * h3lis.convertToG(400,z) + HIGH_IMU_Z_OFFSET);

    Serial.println("Accel Averages:");
    Serial.print(" X = ");
    Serial.println(std::accumulate(accelXBuffer.begin(), accelXBuffer.end(), 0.0) / accelXBuffer.size(), 3);
    Serial.print(" Y = ");
    Serial.println(std::accumulate(accelYBuffer.begin(), accelYBuffer.end(), 0.0) / accelYBuffer.size(), 3);
    Serial.print(" Z = ");
    Serial.println(std::accumulate(accelZBuffer.begin(), accelZBuffer.end(), 0.0) / accelZBuffer.size(), 3);
  }
  
  uint32_t task_lowIMUsend() {
    int16_t accelerometer[3];
    int16_t gyroscope[3];

    if (lms6d.Get_X_AxesRaw(accelerometer)){
      Serial.println("Error reading accelerometer");
      return 200 * 1000;
    }
    if (lms6d.Get_G_AxesRaw(gyroscope)){
      Serial.println("Error reading gyroscope");
      return 200 * 1000;
    }
    
    int32_t accelFS;
    lms6d.Get_X_FS(&accelFS);
    int32_t gyroFS;
    lms6d.Get_G_FS(&gyroFS);
    
    Serial.print("accelFS: ");
    Serial.println(accelFS);
    Serial.print("gyroFS: ");
    Serial.println(gyroFS);

    Serial.print("\nAccelerometer:\n");
    Serial.print(" X = ");
    Serial.println(convertAccel(accelerometer[0], accelFS), 3);
    Serial.print(" Y = ");
    Serial.println(convertAccel(accelerometer[1], accelFS), 3);
    Serial.print(" Z = ");
    Serial.println(convertAccel(accelerometer[2], accelFS), 3);
    
    Serial.print("\nGyroscope:\n");
    Serial.print(" X = ");
    Serial.println(convertGyro(gyroscope[0], gyroFS), 3);
    Serial.print(" Y = ");
    Serial.println(convertGyro(gyroscope[1], gyroFS), 3);
    Serial.print(" Z = ");
    Serial.println(convertGyro(gyroscope[2], gyroFS), 3);
    Serial.println(" ");

    return 200 * 1000;
  }
  
  void getLowIMU(float *readings) {
    int16_t accelerometer[3];
    if (lms6d.Get_X_AxesRaw(accelerometer)){
      Serial.println("Error reading accelerometer");
      return;
    }
    int32_t accelFS;
    lms6d.Get_X_FS(&accelFS);
    readings[0] = convertAccel(accelerometer[0], accelFS);
    readings[1] = convertAccel(accelerometer[1], accelFS);
    readings[2] = convertAccel(accelerometer[2], accelFS);
  }

  uint32_t task_highIMUsend() {
    // imuAverages();
    int16_t x, y, z;
    h3lis.readAxes(x, y, z);

    Serial.println(HIGH_IMU_X_MULTIPLIER * h3lis.convertToG(400,x) + HIGH_IMU_X_OFFSET);
    Serial.println(HIGH_IMU_Y_MULTIPLIER * h3lis.convertToG(400,y) + HIGH_IMU_Y_OFFSET);
    Serial.println(HIGH_IMU_Z_MULTIPLIER * h3lis.convertToG(400,z) + HIGH_IMU_Z_OFFSET);
    Serial.println(" ");
    return 200 * 1000;
  }

  void getHighIMU(float *readings) {
    int16_t x, y, z;
    h3lis.readAxes(x, y, z);

    readings[0] = HIGH_IMU_X_MULTIPLIER * h3lis.convertToG(400,x) + HIGH_IMU_X_OFFSET;
    readings[1] = HIGH_IMU_Y_MULTIPLIER * h3lis.convertToG(400,y) + HIGH_IMU_Y_OFFSET;
    readings[2] = HIGH_IMU_Z_MULTIPLIER * h3lis.convertToG(400,z) + HIGH_IMU_Z_OFFSET;
  }

  void resetRotation() {}
}