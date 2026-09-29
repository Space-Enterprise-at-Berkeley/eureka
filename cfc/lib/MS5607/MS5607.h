/*File   : MS5607.h
  Author : Amit Ate
  Email  : amit@uravulabs.com
  Company: Uravu Labs
*/

#define ADC_READ  0X00         // adc read command
#define PROM_READ  0xA0     // prom read command
#define RESET 0x1E          // soft reset command

#include <Arduino.h>

class MS5607
{
  public:
    MS5607(uint8_t cs_pin);
    void begin();
    void setOSR(short OSR_U);
    float getTemperature(void);
    float getPressure(void);
    // void readDigitalValue(void);
    float getAltitude(void);
    void setReferencePressure(float pressure);
    float P0;
    uint8_t CS_PIN;                // Chip Select pin for SPI
    short OSR = 4096;              // default over sampling ratio
    short CONV_D1 = 0x48;          // corresponding temp conv. command for OSR
    short CONV_D2 = 0x58;          // corresponding pressure conv. command for OSR
    uint32_t CONV_DELAY = 10;          // corresponding conv. delay for OSR

    unsigned int C1,C2,C3,C4,C5,C6;
    unsigned long DP, DT;
    float dT, TEMP, P;
    int64_t OFF, SENS;

    void resetDevice(void);
    void transfer(void (*callback)(uint8_t *values, int length), uint8_t *values, int length);
    void readCalibration();
    uint16_t readUInt_16(uint8_t address);
    void readBytes(uint8_t *values, int length);
    
    void convert(uint8_t cmd);
    bool updateConversionCycle();

    unsigned long getDigitalValue(void);

  private:
    uint32_t convStartMs = 0;
    enum ConvState { CONV_IDLE, CONV_WAIT_D1, CONV_WAIT_D2 };
    ConvState convState = CONV_IDLE;
};
