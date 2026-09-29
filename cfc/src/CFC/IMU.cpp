#include "IMU.h"

namespace IMU {
  float_t quaterionViaLow[4];

  LSM6DSOSensor lms6d(&SPI, LOW_IMU_CS_PIN);
  LIS331 h3lis;
  
  std::vector<float> accelXBuffer;
  std::vector<float> accelYBuffer;
  std::vector<float> accelZBuffer;

  Comms::Packet p;
  float maxAccelX;
  float maxAccelY;
  float maxAccelZ;
  float maxAccelNorm;
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
    h3lis.setPowerMode(LIS331::NORMAL);
    h3lis.setODR(LIS331::DR_1000HZ);
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
  
  uint32_t UPDATE_PERIOD_LOW = 50 * 1000;

  int8_t index = 0;
  int8_t samples = 4;

  float accelXSum = 0.0;
  float accelYSum = 0.0;
  float accelZSum = 0.0;
  float gyroXSum = 0.0;
  float gyroYSum = 0.0;
  float gyroZSum = 0.0;

  bool averageIMU = false;

  uint32_t task_lowIMUsend() {
    int16_t accelerometer[3];
    int16_t gyroscope[3];

    if (lms6d.Get_X_AxesRaw(accelerometer)){
      Serial.println("Error reading accelerometer");
      return UPDATE_PERIOD_LOW;
    }
    if (lms6d.Get_G_AxesRaw(gyroscope)){
      Serial.println("Error reading gyroscope");
      return UPDATE_PERIOD_LOW;
    }
    
    int32_t accelFS;
    lms6d.Get_X_FS(&accelFS);
    int32_t gyroFS;
    lms6d.Get_G_FS(&gyroFS);
    
    float accelX = convertAccel(accelerometer[0], accelFS);
    float accelY = convertAccel(accelerometer[1], accelFS);
    float accelZ = convertAccel(accelerometer[2], accelFS);
    float gyroX = convertGyro(gyroscope[0], gyroFS);
    float gyroY = convertGyro(gyroscope[1], gyroFS);
    float gyroZ = convertGyro(gyroscope[2], gyroFS);

    if (averageIMU) {
      Serial.print("IN IF");
      
      accelXSum += accelX;
      accelYSum += accelY;
      accelZSum += accelZ;
      gyroXSum += gyroX;
      gyroYSum += gyroY;
      gyroZSum += gyroZ;

      index++;

      if (index >= samples) {
        PacketLowIMUValues::Builder()
        .withAccelX(accelXSum / samples)
        .withAccelY(accelYSum / samples)
        .withAccelZ(accelZSum / samples)
        .withGyroX(gyroXSum / samples)
        .withGyroY(gyroYSum / samples)
        .withGyroZ(gyroZSum / samples)
        .build()
        .writeRawPacket(&p);
        Comms::emitPacketOverAllInterfaces(&p);

        index = 0;

        accelXSum = 0.0;
        accelYSum = 0.0;
        accelZSum = 0.0;
        gyroXSum = 0.0;
        gyroYSum = 0.0;
        gyroZSum = 0.0;
      }
    } else {
      Serial.print("IN ELSE");

      PacketLowIMUValues::Builder()
      .withAccelX(accelX)
      .withAccelY(accelY)
      .withAccelZ(accelZ)
      .withGyroX(gyroX)
      .withGyroY(gyroY)
      .withGyroZ(gyroZ)
      .build()
      .writeRawPacket(&p);
      Comms::emitPacketOverAllInterfaces(&p);
    }
    
    // Serial.print("accelFS: " );
    // Serial.print(accelFS);
    // Serial.println("gyroFS: ");
    // Serial.print(gyroFS);

    // Serial.print("\nAccelerometer:\n");
    // Serial.print(" X = ");
    // Serial.print(accelX, 3);
    // Serial.print(" Y = ");
    // Serial.print(accelY, 3);
    // Serial.print(" Z = ");
    // Serial.print(accelZ, 3);

    // Serial.print("\nGyroscope:\n");
    // Serial.print(" X = ");
    // Serial.print(gyroX, 3);
    // Serial.print(" Y = ");
    // Serial.print(gyroY, 3);
    // Serial.print(" Z = ");
    // Serial.print(gyroZ, 3);
    // Serial.print(" ");

    return UPDATE_PERIOD_LOW;
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

  uint32_t UPDATE_PERIOD_HIGH = 8000;
  uint32_t lasttime = 0;
  uint32_t task_highIMUsend() {
    int16_t x, y, z;
    h3lis.readAxes(x, y, z);
    float accelX = HIGH_IMU_X_MULTIPLIER * h3lis.convertToG(400,x) + HIGH_IMU_X_OFFSET;
    float accelY = HIGH_IMU_Y_MULTIPLIER * h3lis.convertToG(400,y) + HIGH_IMU_Y_OFFSET;
    float accelZ = HIGH_IMU_Z_MULTIPLIER * h3lis.convertToG(400,z) + HIGH_IMU_Z_OFFSET;
    if(micros()-lasttime >= 1000 * 1000){
      PacketHighIMUValues::Builder()
        .withAccelX(maxAccelX)
        .withAccelY(maxAccelY)
        .withAccelZ(maxAccelZ)
        .build()
        .writeRawPacket(&p);
      Comms::emitPacketOverAllInterfaces(&p);
      lasttime = micros();
      maxAccelX = 0;
      maxAccelY = 0;
      maxAccelZ = 0;
      maxAccelNorm = 0;
    }
    if(maxAccelNorm < sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ)){
      maxAccelNorm = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);
      maxAccelX = accelX;
      maxAccelY = accelY;
      maxAccelZ = accelZ;
    }
    Serial.println(accelX, 6);
    Serial.println(accelY, 6);
    Serial.println(accelZ, 6);
    Serial.println(" ");
    
    

    return UPDATE_PERIOD_HIGH;
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