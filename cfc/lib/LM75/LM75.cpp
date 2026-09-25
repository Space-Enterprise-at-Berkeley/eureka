/*
    LM75 - An arduino library for the LM75 temperature sensor
    Copyright (C) 2011  Dan Fekete <thefekete AT gmail DOT com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "LM75.h"
#include "freertos/FreeRTOS.h"

LM75::LM75 () {
  address = LM75_ADDRESS;
  m_port = I2C_NUM_0;
}

LM75::LM75 (uint8_t addr, i2c_port_t port) {
  address = addr;
  m_port = port;
}

uint16_t LM75::float2regdata (float temp)
{
  // First multiply by 8 and coerce to integer to get +/- whole numbers
  // Then coerce to word and bitshift 5 to fill out MSB
  return (uint16_t)((int)(temp * 8) << 5);
}

float LM75::regdata2float (uint16_t regdata)
{
  return ((float)(int)regdata / 32) / 8;
}

uint16_t LM75::_register16 (uint8_t reg) {
  uint8_t buf[2] = {0};
  i2c_master_write_read_device(m_port, address, &reg, 1, buf, 2,
                               pdMS_TO_TICKS(100));
  return ((uint16_t)buf[0] << 8) | buf[1];
}

void LM75::_register16 (uint8_t reg, uint16_t regdata) {
  uint8_t msb = (uint8_t)(regdata >> 8);
  uint8_t lsb = (uint8_t)(regdata);

  uint8_t buf[3] = {reg, msb, lsb};
  i2c_master_write_to_device(m_port, address, buf, 3, pdMS_TO_TICKS(100));
}

uint8_t LM75::_register8 (uint8_t reg) {
  uint8_t value = 0;
  i2c_master_write_read_device(m_port, address, &reg, 1, &value, 1,
                               pdMS_TO_TICKS(100));
  return value;
}

void LM75::_register8 (uint8_t reg, uint8_t regdata) {
  uint8_t buf[2] = {reg, regdata};
  i2c_master_write_to_device(m_port, address, buf, 2, pdMS_TO_TICKS(100));
}

float LM75::temp (void) {
  return regdata2float(_register16(LM75_TEMP_REGISTER));
}

uint8_t LM75::conf () {
  return _register8(LM75_CONF_REGISTER);
}

void LM75::conf (uint8_t data) {
  _register8(LM75_CONF_REGISTER, data);
}

float LM75::tos () {
  return regdata2float(_register16(LM75_TOS_REGISTER));
}

void LM75::tos (float temp) {
  _register16(LM75_TOS_REGISTER, float2regdata(temp));
}

float LM75::thyst () {
  return regdata2float(_register16(LM75_THYST_REGISTER));
}

void LM75::thyst (float temp) {
  _register16(LM75_THYST_REGISTER, float2regdata(temp));
}

bool LM75::shutdown () {
  return conf() & 0x01;
}

void LM75::shutdown (bool val) {
  conf(val << LM75_CONF_SHUTDOWN);
}
