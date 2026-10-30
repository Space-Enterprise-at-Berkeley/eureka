#include "PT.h"
#include "Common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// reads the PT channels from the ADS8167 ADC (U14) and prints them over serial
namespace PT {
uint32_t UPDATE_PERIOD = 500 * 1000; // 2 Hz

ADS8167 adc;

void init() {
  // SPI.begin(17, 16, 15) is done in main, so ADS8167::init's SPI begin is a
  // no-op
  adc.init(&SPI, PT_ADC_CS_PIN, PT_ADC_READY_PIN);
  adc.setAllInputsSeparate();
  adc.setManualMode();
}

void vTaskReadPT(void *pvParameters) {
  (void)pvParameters;
  char line[256];
  TickType_t lastWake = xTaskGetTickCount();
  while (1) {
    // READY is high whenever the ADC is idle. If it's low here it's stuck (ADC
    // unpowered or a bad joint), and the driver would spin on it forever, so
    // skip this read instead
    if (digitalRead(PT_ADC_READY_PIN) == LOW) {
      snprintf(line, sizeof(line), "PT: READY_ADC stuck low, skipping read\n");
    } else {
      uint16_t raw[PT_ADC_CHANNELS];
      for (uint8_t ch = 0; ch < PT_ADC_CHANNELS; ch++) {
        raw[ch] = adc.readData(ch);
      }

      int n = snprintf(line, sizeof(line), "PT:");
      for (uint8_t ch = 0; ch < PT_ADC_CHANNELS; ch++) {
        n += snprintf(line + n, sizeof(line) - n, " AIN%u %u (%.4f V)", ch,
                      raw[ch], raw[ch] * PT_ADC_VREF / 65535.0);
      }
      snprintf(line + n, sizeof(line) - n, "\n");
    }

    // hold the emit lock so this text can't land in the middle of a packet on
    // USB serial
    xSemaphoreTake(Comms::emitLock, portMAX_DELAY);
    Serial.print(line);
    xSemaphoreGive(Comms::emitLock);

    taskDelayUntil(&lastWake, pdMS_TO_TICKS(UPDATE_PERIOD / 1000));
  }
}
} // namespace PT
