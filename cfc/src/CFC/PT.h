#pragma once

#include <ADS8167.h>
#include <Arduino.h>
#include <Comms.h>
#include <SPI.h>

#define PT_ADC_CS_PIN 35    // CS_ADC, U1 pin 31
#define PT_ADC_READY_PIN 36 // READY_ADC, U1 pin 32
#define PT_ADC_VREF 5.0f    // REFIO is tied to P5V0_MAIN
#define PT_ADC_CHANNELS 8   // AIN0-3 are PT0-3, AIN4-7 are the spare inputs

// reads the PT channels from the ADS8167 ADC (U14) and prints them over serial
namespace PT {
void init();
void vTaskReadPT(void *pvParameters);
} // namespace PT
