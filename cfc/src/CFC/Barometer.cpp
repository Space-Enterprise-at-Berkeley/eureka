#include "Barometer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "barometer";

namespace Barometer {
uint8_t cs = 37;
uint32_t UPDATE_PERIOD = 50 * 1000; // 20 Hz
MS5607 P_Sens = MS5607(cs);

float baroAltitude, baroPressure, baroTemperature;
float sumPressure = 0;
int pressureSamples = 0;
float finalAvgPressure = 0;
bool calibration = false;
bool calibrationcompleted = false;

void init(void) {
  P_Sens.begin();
  Comms::registerCallback(PACKET_ID_FCEnableBaroCalibration, en_baro_callback);
  ESP_LOGI(TAG, "MS5607 initialized");
}

void vTaskSampleBaro(void *pvParameters) {
  (void)pvParameters;
  Comms::Packet p;
  while (1) {
    if (!P_Sens.updateConversionCycle()) {
      vTaskDelay(pdMS_TO_TICKS(P_Sens.CONV_DELAY));
      continue;
    }
    baroPressure = P_Sens.getPressure();
    baroTemperature = P_Sens.getTemperature();
    baroAltitude = P_Sens.getAltitude();

    // only this task shall poll, so it must accumulate as well
    if (calibration && !calibrationcompleted) {
      sumPressure += baroPressure;
      pressureSamples++;
    }

    PacketBaroValues::Builder()
        .withAltitude(baroAltitude)
        .withPressure(baroPressure)
        .withTemperature(baroTemperature)
        .build()
        .writeRawPacket(&p);
    Comms::emitPacketOverAllInterfaces(&p);
    vTaskDelay(
        pdMS_TO_TICKS((UPDATE_PERIOD - P_Sens.CONV_DELAY * 2 * 1000) / 1000));
  }
}

void vTaskAverageBaro(void *pvParameters) {
  (void)pvParameters;
  Comms::Packet p;
  while (1) {
    float avgPressure = finalAvgPressure;
    if (!calibrationcompleted && pressureSamples > 0) {
      avgPressure = sumPressure / pressureSamples;
    }

    PacketAvgBaroValues::Builder()
        .withAvgpressure(avgPressure)
        .build()
        .writeRawPacket(&p);
    Comms::emitPacketOverAllInterfaces(&p);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void en_baro_callback(Comms::Packet packet, uint8_t ip) {
  PacketFCEnableBaroCalibration parsed_packet =
      PacketFCEnableBaroCalibration::fromRawPacket(&packet);
  bool enable = parsed_packet.m_Action;
  if (enable && !calibrationcompleted) {
    calibration = true;
    calibrationcompleted = false;
    sumPressure = 0;
    pressureSamples = 0;
    finalAvgPressure = 0;
  } else if (!enable && !calibrationcompleted) {
    calibration = false;
    calibrationcompleted = true;
    if (pressureSamples > 0 && sumPressure > 0) {
      finalAvgPressure = sumPressure / pressureSamples;
      P_Sens.setReferencePressure(finalAvgPressure);
    }
  }
}
} // namespace Barometer
