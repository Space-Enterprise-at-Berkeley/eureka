/**************************************************************************/
/*!
  @file     infinityPV_INA233.cpp
  @author   rava (infinityPV ApS)
	@license  BSD (see license.txt)

	This is a library for all the infinityPV boards using the ADC/I2C converter
  INA233:
  - 1. LiBAT5V2A Energy Management Shield
  ----> https://infinitypv.com/products/electronics/libat5v2a

	InfinityPV makes printed organic solar cells. We invests part of our
  time and resources providing open source examples (hardware and code) to
  make easier the use of our solar cells in your projects.
	Printed organic solar cells are flexible, rollable and super light unlike
  other solar cell technologies, please visit our website to find more !

  (*)This libray is protected by BSD license, Copyright (c) 2018, infinityPV ApS
  All rights reserved
	@section  HISTORY

    v1.0  - First release Mar 2018
*/
/**************************************************************************/
#include <cmath>

#include "INA233.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "ina233";

static esp_err_t inaWrite(i2c_port_t port, uint8_t addr, const uint8_t *data,
                          size_t len) {
  return i2c_master_write_to_device(port, addr, data, len,
                                    pdMS_TO_TICKS(100));
}

static esp_err_t inaReadReg(i2c_port_t port, uint8_t addr, uint8_t reg,
                            uint8_t *data, size_t len) {
  return i2c_master_write_read_device(port, addr, &reg, 1, data, len,
                                      pdMS_TO_TICKS(100));
}
/**************************************************************************/
/*!
    @brief  Writes a byte over I2C, no data are sent, only the
    PMBus comand (reg).
*/
/**************************************************************************/
void INA233::wireSendCmd(uint8_t reg)
{
  inaWrite(m_port, ina233_i2caddr, &reg, 1);
}

/**************************************************************************/
/*!
    @brief  Writes a byte (value) to the specified register
    by the PMBus comand (reg) over I2C
*/
/**************************************************************************/
void INA233::wireWriteByte (uint8_t reg, uint8_t value)
{
  const uint8_t buf[2] = {reg, value};
  inaWrite(m_port, ina233_i2caddr, buf, sizeof(buf));
}
/**************************************************************************/
/*!
    @brief  Writes a word (value) to the specified register
    by the PMBus comand (reg) over I2C
*/
/**************************************************************************/
void INA233::wireWriteWord (uint8_t reg, uint16_t value)
{
  const uint8_t buf[3] = {reg, (uint8_t)(value & 0xFF),
                          (uint8_t)((value >> 8) & 0xFF)};
  inaWrite(m_port, ina233_i2caddr, buf, sizeof(buf));
}
/**************************************************************************/
/*!
    @brief  Reads a Block of 6 bytes from the INA233 over I2C
    When reading a block in PMBUS the first byte from the slave is the
    block size (6 in this case), so the request must be for block_size+1
*/
/**************************************************************************/
void INA233::wireReadBlock(uint8_t reg, uint8_t value[6])
{
  uint8_t buf[7] = {0};
  inaReadReg(m_port, ina233_i2caddr, reg, buf, sizeof(buf));
  uint8_t block_size = buf[0];
  for (uint8_t i = 0; i < block_size && i < 6; i++)
  {
    value[i] = buf[i + 1];
  }
}

/**************************************************************************/
/*!
    @brief  Reads a 16 bit value from the INA233 over I2C
*/
/**************************************************************************/
void INA233::wireReadWord(uint8_t reg, uint16_t *value)
{
  uint8_t buf[2] = {0};
  inaReadReg(m_port, ina233_i2caddr, reg, buf, sizeof(buf));
  *value = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}
/**************************************************************************/
/*!
    @brief  Reads a 8 bit value from the INA233 over I2C
*/
/**************************************************************************/
void INA233::wireReadByte(uint8_t reg, uint8_t *value)
{
  inaReadReg(m_port, ina233_i2caddr, reg, value, 1);
}
/**************************************************************************/
/*!
    @brief  Set INA233 Calibration register for measuring based on the user's
    inputs r_shunt and i_max.
    -inputs: value of the shunt resistor and maximum current (in ohms and A)
    -inputs as outputs: measuring accuracy for current (uA) and power (mW) and
    ERROR state for possible errors during Calibration.
    -outputs: the CAL value to be written in MFR_CALIBRATION

    */
/**************************************************************************/
uint16_t INA233::setCalibration(float r_shunt,float i_max, uint8_t *ERROR)
{
  float C_LSB=0;
  float P_LSB=0;
  float CAL=0;
  float m_c_F=0;
  float m_p_F=0;
  int32_t aux=0;
  bool round_done=false;
  int8_t local_R_c=0;
  int8_t local_R_p=0;
  uint8_t local_ERROR=0;

  C_LSB=i_max/pow(2,15);
  P_LSB=25*C_LSB;
  Current_LSB=C_LSB*1000000;
  Power_LSB=P_LSB*1000;
  CAL=0.00512/(r_shunt*C_LSB);

  //Check CAL is in the uint16 range
  if (CAL>0xFFFF)
    {
      local_ERROR=1;
    }
  else
    {
    wireWriteWord(MFR_CALIBRATION, (uint16_t)CAL);
    }
  m_c_F=1/C_LSB;
  m_p_F=1/P_LSB;

  //Calculate m and R for maximum accuracy in current measurement
  aux=(int32_t)m_c_F;
  while ((aux>32768)||(aux<-32768))
    {
      m_c_F=m_c_F/10;
      local_R_c++;
      aux=(int32_t)m_c_F;
    }
  while (round_done==false)
    {
      aux=(int32_t)m_c_F;
      if (aux==m_c_F)
      {
        round_done=true;
      }
      else
      {
         aux=(int32_t)(m_c_F*10);             //shift decimal to the right
         if ((aux>32768)||(aux<-32768))       //m_c is out of int16 (-32768 to 32768)
         {
          round_done=true;
         }
         else
         {
          m_c_F=m_c_F*10;
          local_R_c--;
         }
      }
    }
  round_done=false;
  //Calculate m and R for maximum accuracy in power measurement
  aux=(int32_t)m_p_F;
  while ((aux>32768)||(aux<-32768))
    {
      m_p_F=m_p_F/10;
      local_R_p++;
      aux=(int32_t)m_p_F;
    }
  while (round_done==false)
    {
      aux=(int32_t)m_p_F;
      if (aux==m_p_F)
      {
        round_done=true;
      }
      else
      {
         aux=(int32_t)(m_p_F*10);          //shift decimal to the right
         if ((aux>32768)||(aux<-32768))       //m_p is out of int16 (-32768 to 32768)
         {
          round_done=true;
         }
         else
         {
          m_p_F=m_p_F*10;
          local_R_p--;
         }
      }
    }
  *ERROR=local_ERROR;

  m_c=int16_t(m_c_F);
  m_p=int16_t(m_p_F);
  R_c=local_R_c;
  R_p=local_R_p;

  return(uint16_t)CAL;
}
/**************************************************************************/
/*!
    @brief  Instantiates a new INA233 class
*/
/**************************************************************************/
INA233::INA233(uint8_t addr, i2c_port_t port) : m_port(port) {
  ina233_i2caddr = addr;
  Current_LSB = 0;
  Power_LSB = 0;
  m_c=0;
  R_c=0;
  m_p=0;
  R_p=0;
}

uint16_t INA233::init(float r_shunt = 0.004, float i_max = 10){
  uint16_t CAL=0;
  uint8_t Set_ERROR=0;
  CAL = setCalibration(r_shunt,i_max,&Set_ERROR);
  if (Set_ERROR==1)
  {
    ESP_LOGE(TAG, "Calibration value is out of range");
  }
  else
  {
    ESP_LOGI(TAG,
             "Calibration value: %u, Current LSB: %.3f uA, Power LSB: %.3f mW, "
             "m_c: %d, R_c: %d, m_p: %d, R_p: %d",
             (unsigned)CAL, Current_LSB, Power_LSB, m_c, R_c, m_p, R_p);
  }
  return CAL;
}

/**************************************************************************/
/*!
    @brief  Gets the raw bus voltage (2-byte, two's complement integer
    received from the device)
*/
/**************************************************************************/
int16_t INA233::readBusVoltage_raw() {
  uint16_t value;
  wireReadWord(READ_VIN, &value);

  return (int16_t)value;
}
/**************************************************************************/
/*!
    @brief  Gets the raw shunt voltage (2-byte, two's complement integer
    received from the device)
*/
/**************************************************************************/
int16_t INA233::readShuntVoltage_raw() {
  uint16_t value;
  wireReadWord(MFR_READ_VSHUNT, &value);
  return (int16_t)value;
}
/**************************************************************************/
/*!
    @brief  reads the raw current value (2-byte, two's complement integer
    received from the device)
*/
/**************************************************************************/
int16_t INA233::readCurrent_raw() {
  uint16_t value;
  wireReadWord(READ_IIN, &value);
  return (int16_t)value;
}
/**************************************************************************/
/*!
    @brief  reads the raw power value (2-byte, two's complement integer
    received from the device)
*/
/**************************************************************************/
int16_t INA233::readPower_raw() {
  uint16_t value;
  wireReadWord(READ_PIN, &value);
  return (int16_t)value;
}
/**************************************************************************/
/*!
    @brief  reads the raw energy info from READ_EIN register power accumulator
    (2-byte), power accumulator roll over (1byte) and sample count (3bytes)

*/
/**************************************************************************/
void INA233::readEnergy_raw(uint16_t* accumulator, uint8_t* roll_over, uint32_t* sample_count) {
  uint8_t value[6];
  //uint8_t test[6] = { 0x00, 0x11,0x22,0x33,0x44,0x55};
  // uint32_t aux;
  wireReadBlock(READ_EIN, value);
  *accumulator=(value[1] << 8) | value[0];
  *roll_over=value[2];
  *sample_count=uint32_t(value[5])<< 16;
  *sample_count=((uint32_t(value[4])<< 8)| *sample_count);
  *sample_count=(uint32_t(value[3])| *sample_count);

}
/**************************************************************************/
/*!
    @brief  reads the averaged power from last reading of READ_EIN in mW
*/
/**************************************************************************/
float INA233::readAvgPower() {
  uint16_t accumulator=0;
  uint8_t roll_over=0;
  uint32_t sample_count=0;
  uint32_t accumulator_24=0;
  uint32_t raw_av_power=0;
  float av_power=0;
  readEnergy_raw(&accumulator,&roll_over, &sample_count);
  accumulator_24=uint32_t(roll_over)*65536+uint32_t(accumulator);
  raw_av_power=accumulator_24/sample_count;

  av_power=(raw_av_power*pow(10,-R_p)-b_p)/m_p;
  return av_power * 1000;
}
/**************************************************************************/
/*!
    @brief  reads the shunt voltage in mV
*/
/**************************************************************************/
float INA233::readShuntVoltage() {
  int16_t value=readShuntVoltage_raw();
  float vshunt;
  vshunt=(value*pow(10,-R_vs)-b_vs)/m_vs;
  return vshunt * 1000;
}

/**************************************************************************/
/*!
    @brief  reads the shunt voltage in volts
*/
/**************************************************************************/
float INA233::readBusVoltage() {
  int16_t value=readBusVoltage_raw();
  float vbus;
  vbus =(value*pow(10,-R_vb)-b_vb)/m_vb;
  return vbus;
}

/**************************************************************************/
/*!
    @brief  reads the current value in mA, taking into account the
            config settings and current LSB
*/
/**************************************************************************/
float INA233::readCurrent() {
  int16_t value=readCurrent_raw();
  float current;
  current =(value*pow(10,-R_c)-b_c)/m_c;
  return current*1000;
}

/**************************************************************************/
/*!
    @brief  reads the power value in mW, taking into account the
            config settings and power LSB
*/
/**************************************************************************/
float INA233::readPower() {
  int16_t value=readPower_raw();
  float power;
  power =(value*pow(10,-R_p)-b_p)/m_p;
  return power*1000;
}


/**************************************************************************/
/*!
    @brief  Gets the Current Least-Significant-Bit value
*/
/**************************************************************************/
float INA233::getCurrent_LSB(void){
  return Current_LSB;
}

/**************************************************************************/
/*!
    @brief  Gets the Power Least-Significant-Bit value
*/
/**************************************************************************/
float INA233::getPower_LSB(void){
  return Power_LSB;
}

/**************************************************************************/
/*!
    @brief  Gets the current m_c value where current is calculated as
            current = (value*pow(10,-R_c)-b_c)/m_c
*/
/**************************************************************************/
int16_t INA233::get_m_c(void){
  return m_c;
}

/**************************************************************************/
/*!
    @brief  Gets the current R_c value where current is calculated as
            current = (value*pow(10,-R_c)-b_c)/m_c
*/
/**************************************************************************/
int8_t INA233::get_R_c(void){
  return R_c;
}

/**************************************************************************/
/*!
    @brief  Gets the current m_p value where power is calculated as
            power = (value*pow(10,-R_p)-b_p)/m_p
*/
/**************************************************************************/
int16_t INA233::get_m_p(void){
  return m_p;
}
/**************************************************************************/
/*!
    @brief  Gets the current R_p value where power is calculated as
            power = (value*pow(10,-R_p)-b_p)/m_p
*/
/**************************************************************************/
int8_t INA233::get_R_p(void){
  return R_p;
}

