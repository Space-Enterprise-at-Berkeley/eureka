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

#pragma once

#include "driver/i2c.h"
#include <cstdint>

#define LM75_ADDRESS 0x48

#define LM75_TEMP_REGISTER 0
#define LM75_CONF_REGISTER 1
#define LM75_THYST_REGISTER 2
#define LM75_TOS_REGISTER 3

#define LM75_CONF_SHUTDOWN  0
#define LM75_CONF_OS_COMP_INT 1
#define LM75_CONF_OS_POL 2
#define LM75_CONF_OS_F_QUE 3

class LM75 {
    int address;
    i2c_port_t m_port;
    uint16_t float2regdata (float);
    float regdata2float (uint16_t);
    uint16_t _register16 (uint8_t);
    void _register16 (uint8_t, uint16_t);
    uint8_t _register8 (uint8_t);
    void _register8 (uint8_t, uint8_t);
  public:
    LM75 ();
    LM75 (uint8_t, i2c_port_t port = I2C_NUM_0);
    float temp (void);
    uint8_t conf (void);
    void conf (uint8_t);
    float tos (void);
    void tos (float);
    float thyst (void);
    void thyst (float);
    void shutdown (bool);
    bool shutdown (void);
};
