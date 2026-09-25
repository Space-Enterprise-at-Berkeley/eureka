#include <math.h>
#include <MS5607.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ms5607";

MS5607::MS5607(uint8_t cs_pin, spi_host_device_t host)
{
  this->CS_PIN = cs_pin;
  this->m_host = host;
  this->P0 = 1013.25; // default reference pressure in mBar
}

// Initialise the SPI device and read calibration data
void MS5607::begin()
{
  spi_device_interface_config_t devcfg = {};
  devcfg.clock_speed_hz = 1000000; // 1 MHz
  devcfg.mode = 0;
  devcfg.spics_io_num = CS_PIN;
  devcfg.queue_size = 1;
  ESP_ERROR_CHECK(spi_bus_add_device(m_host, &devcfg, &m_dev));

  readCalibration();
}
void MS5607::setReferencePressure(float pressure) {
  // Set the reference pressure for altitude calculations
  this->P0 = pressure;
}
void MS5607::transfer(const uint8_t *tx, uint8_t *rx, size_t length)
{
  spi_transaction_t transaction = {};
  transaction.length = length * 8;
  transaction.tx_buffer = tx;
  transaction.rx_buffer = rx;
  ESP_ERROR_CHECK(spi_device_transmit(m_dev, &transaction));
}

void MS5607::resetDevice(void) 
{
  const uint8_t cmd = RESET;
  transfer(&cmd, nullptr, 1);
}

// read calibration data from PROM
void MS5607::readCalibration()
{
  resetDevice();
  vTaskDelay(pdMS_TO_TICKS(3));
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
  uint8_t tx[3] = {address, 0x00, 0x00};
  uint8_t rx[3] = {0};
  transfer(tx, rx, sizeof(tx));
  return (((unsigned int) rx[1] * (1 << 8)) | (unsigned int) rx[2]);
}

/** Read length bytes from the device into values array, using values[0] as the command byte. */
void MS5607::readBytes(uint8_t *values, int length)
{
  uint8_t tx[8] = {0};
  uint8_t rx[8] = {0};
  if (length > 7) {
    length = 7;
  }
  tx[0] = values[0];
  transfer(tx, rx, length + 1);
  for (int i = 0; i < length; i++) {
    values[i] = rx[i + 1];
  }
}


// send command to start conversion of temp/pressure
void MS5607::convert(uint8_t cmd)
{
  transfer(&cmd, nullptr, 1);
  convStartUs = esp_timer_get_time();
}

bool MS5607::updateConversionCycle()
{
  switch (convState) {
    case CONV_IDLE:
      convert(CONV_D1);
      convState = CONV_WAIT_D1;
      return false;

    case CONV_WAIT_D1:
      if (esp_timer_get_time() - convStartUs < (int64_t)CONV_DELAY * 1000) {
        ESP_LOGD(TAG, "early!");
        return false;
      }
      DP = getDigitalValue();
      convert(CONV_D2);
      convState = CONV_WAIT_D2;
      return false;

    case CONV_WAIT_D2:
      if (esp_timer_get_time() - convStartUs < (int64_t)CONV_DELAY * 1000) {
        ESP_LOGD(TAG, "early!");
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
  uint8_t tx[4] = {ADC_READ, 0x00, 0x00, 0x00};
  uint8_t rx[4] = {0};
  transfer(tx, rx, sizeof(tx));
  unsigned long value = (unsigned long) rx[1] * 1 << 16 |
                        (unsigned long) rx[2] * 1 << 8 |
                        (unsigned long) rx[3];
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
