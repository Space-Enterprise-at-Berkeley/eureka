#include <math.h>
#include <MS5607.h>
#include <Wire.h>
#include <SPI.h>

MS5607::MS5607(uint8_t cs_pin)
{
  this->CS_PIN = cs_pin;
  this->P0 = 1013.25; // default reference pressure in mBar
}

// Initialise coefficient by reading calibration data
void MS5607::begin()
{
  SPI.begin(17,16,15); // SCK, MISO, MOSI
  readCalibration();
}
void MS5607::setReferencePressure(float pressure) {
  // Set the reference pressure for altitude calculations
  this->P0 = pressure;
}
void MS5607::transfer(void (*callback)(uint8_t *values, int length), uint8_t *values, int length)
{
  digitalWrite(CS_PIN, LOW);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  callback(values, length);
  SPI.endTransaction();
  digitalWrite(CS_PIN, HIGH);
}

void MS5607::resetDevice(void) 
{
  transfer([](uint8_t *values, int length) -> void { SPI.transfer(RESET); }, nullptr, 0);
}

// read calibration data from PROM
void MS5607::readCalibration()
{
  resetDevice();
  delay(3);
  C1 = readUInt_16(PROM_READ + 2);
  C2 = readUInt_16(PROM_READ + 4);
  C3 = readUInt_16(PROM_READ + 6);
  C4 = readUInt_16(PROM_READ + 8);
  C5 = readUInt_16(PROM_READ + 10);
  C6 = readUInt_16(PROM_READ + 12);
}

// convert raw data into unsigned int
uint16_t MS5607::readUInt_16(uint8_t address)
{
  uint8_t data[2];
  data[0] = address;
  readBytes(data, 2);
  return (((unsigned int) data[0] * (1 << 8)) | (unsigned int) data[1]);
}

/** Read length bytes from the device into values array, using values[0] as the command byte. */
void MS5607::readBytes(uint8_t *values, int length)
{
  auto read = [](uint8_t *values, int length) 
  {
    SPI.transfer(values[0]);
    for (int i = 0; i < length; i++) {
      values[i] = SPI.transfer(0x00);
    }
  };
  transfer(read, values, length);
}


// send command to start conversion of temp/pressure
void MS5607::convert(uint8_t cmd)
{
  uint8_t values[1] = { cmd };
  transfer([](uint8_t *values, int length) -> void { SPI.transfer(values[0]); }, values, 1);
  
  convStartMs = millis();
}

bool MS5607::updateConversionCycle()
{
  switch (convState) {
    case CONV_IDLE:
      convert(CONV_D1);
      convState = CONV_WAIT_D1;
      return false;

    case CONV_WAIT_D1:
      if (millis() - convStartMs < CONV_DELAY) {
        Serial.println("early!");
        return false;
      }
      DP = getDigitalValue();
      convert(CONV_D2);
      convState = CONV_WAIT_D2;
      return false;

    case CONV_WAIT_D2:
      if (millis() - convStartMs < CONV_DELAY) {
        Serial.println("early!");
        return false;
      }
      DT = getDigitalValue();
      convState = CONV_IDLE;
      return true;
  }
  return false;
}

// read raw digital values of temp & pressure from MS5607
// void MS5607::readDigitalValue(void)
// {
//   startConversion(CONV_D1);
//   DP = getDigitalValue();
//   startConversion(CONV_D2);
//   DT = getDigitalValue();
// }

unsigned long MS5607::getDigitalValue(void) 
{
  uint8_t data[3];
  data[0] = ADC_READ;
  readBytes(data, 3);
  unsigned long value = (unsigned long) data[0] * 1 << 16 | (unsigned long) data[1] * 1 << 8 | (unsigned long) data[2];
  return value;
}

float MS5607::getTemperature(void)
{
  dT = (float)DT - ((float)C5)*((int)1<<8);
  TEMP = 2000.0 + dT * ((float)C6)/(float)((long)1<<23);
  return TEMP/100 ;
}

float MS5607::getPressure(void)
{
  dT = (float)DT - ((float)C5)*((int)1<<8);
  TEMP = 2000.0 + dT * ((float)C6)/(float)((long)1<<23);
  OFF = (((int64_t)C2)*((long)1<<17)) + dT * ((float)C4)/((int)1<<6);
  SENS = ((float)C1)*((long)1<<16) + dT * ((float)C3)/((int)1<<7);
  float pa = (float)((float)DP/((long)1<<15));
  float pb = (float)(SENS/((float)((long)1<<21)));
  float pc = pa*pb;
  float pd = (float)(OFF/((float)((long)1<<15)));
  P = pc - pd;
  return P/100;
}

// set OSR and select corresponding values for conversion commands & delay
void MS5607::setOSR(short OSR_U)
{
  this->OSR = OSR_U;
  switch (OSR) {
    case 256:
      CONV_D1 = 0x40;
      CONV_D2 = 0x50;
      CONV_DELAY = 1;
      break;
    case 512:
      CONV_D1 = 0x42;
      CONV_D2 = 0x52;
      CONV_DELAY = 2;
      break;
    case 1024:
      CONV_D1 = 0x44;
      CONV_D2 = 0x54;
      CONV_DELAY = 3;
      break;
    case 2048:
      CONV_D1 = 0x46;
      CONV_D2 = 0x56;
      CONV_DELAY = 5;
      break;
    case 4096:
      CONV_D1 = 0x48;
      CONV_D2 = 0x58;
      CONV_DELAY = 10;
      break;
    default:
      CONV_D1 = 0x40;
      CONV_D2 = 0x50;
      CONV_DELAY = 1;
      break;
  }
}

float MS5607::getAltitude(void)
{
  float h,t,p;
  t = getTemperature();
  p = getPressure();
  p = P0/p;
  h = 153.84615*(pow(p,0.19) - 1)*(t+273.15);
  return h;
}
