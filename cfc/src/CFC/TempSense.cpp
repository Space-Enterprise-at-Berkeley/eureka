#include "TempSense.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "proto/Packet_FCTemperature.h"

static const char *TAG = "tempsense";

// reads temp stats from LM75
namespace TempSense {
LM75 lm(0x4F);

void init() {
  // The I2C bus is initialized once in prvSetupHardware(); nothing to do here.
}

void vTaskReadSendTemp(void *pvParameters) {
  (void)pvParameters;
  Comms::Packet p;
  while (1) {
    float tC = lm.temp();

    PacketFCTemperature::Builder().withTemp(tC).build().writeRawPacket(&p);
    Comms::emitPacketOverAllInterfaces(&p);

    vTaskDelay(pdMS_TO_TICKS(500)); // .5 second
  }
}

void print() {
  float tC = lm.temp();
  ESP_LOGI(TAG, "Temp: %.2f C", tC);
}
} // namespace TempSense
