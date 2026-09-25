/*
 * Common Flight Computer Serial Diagnostics
 * Just a bunch of simple diagnostic scripts packed into a namespace. 
 *
 * ATTENTION: FCDiagnostics doesn't include the requisite setup routines for all the hardware interfaces on the FC.
 * Refer to the original implementation in Flight-Firmware/src/diagnostic for setup
 */


#include <Arduino.h>
#include <Wire.h>
#include "Common.h"
#include <SPI.h>

#include <INA233.h>

#define ADDRESS_TEMPSENSOR 0x4F
#define INA_SHUNT_RESISTANCE 0.004

#define MID_ACCEL_CS_PIN 40
#define BARO_CS_PIN 5
#define GPS_CS_PIN 3
#define HIGH_ACCEL_CS_PIN 39
#define ADC_CS_PIN 37
#define EXT_DR_CS_PIN 10
#define OBD_DR_CS_PIN 36

#define SENSORSPI_MOSI 15
#define SENSORSPI_MISO 16
#define SENSORSPI_SCLK 17

// --- H3LIS331DL register map & constants ---
#define H3_WHO_AM_I_REG     0x0F
#define H3_WHO_AM_I_VAL     0x32
#define H3_CTRL_REG1        0x20
#define H3_OUT_X_L          0x28
#define H3_OUT_Y_L          0x2A
#define H3_OUT_Z_L          0x2C

#ifndef FCDIAG_H
#define FCDIAG_H

namespace FCDiagnostics {
	
	//SPIClass* sensorSPIbus;
	static const int spiClk = 1000000; 
	
	void runI2CScan(int argc, char **argv);
	void runTemperatureSensorTest(int argc, char **argv);
	void runPowerMonitorTest(INA233 &ina);
	void spamSPIBus(int argc, char **argv, SPIClass* sensorSPIbus);
	void setGPIO(int argc, char **argv);
	void runHighAccelTest(int argc, char **argv, SPIClass* sensorSPIbus);
	void getTlmRadioVersionInConfig(int argc, char **argv);
	void runBarometerTest(int argc, char **argv);
	void runObdFlashTest(int argc, char **argv);
	
	// helpers
	static int16_t h3_read16(uint8_t addr_l);
	static void h3_writeReg(uint8_t reg, uint8_t val);
	static uint8_t h3_readReg(uint8_t reg);
	
	void setSensorSPI(SPIClass* input);
	void setSerial(HardwareSerial* tlmSerial, HardwareSerial* lvSerial);
	
}

#endif //FCDIAG_H
