#include "GPS.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "proto/Packet_GPSExtraValues.h"
#include "proto/Packet_GPSSatInfo.h"
#include "proto/Packet_GPSValues.h"
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <array>
#include <u-blox_structs.h>

static const char *TAG = "gps";

#define GPS_I2C_ADDR 0x42

SFE_UBLOX_GNSS myGNSS;

static SemaphoreHandle_t gpsLock;

namespace GPS {
uint8_t SIV_f = 0;
float latitude_deg, longitude_deg, altitude_m;
float groundSpeed_mps;
float speedAccuracy_mps;
float heading_deg, headingAccuracy_deg;
float hAcc_m, vAcc_m, pAcc_m;
float ATTroll_deg, ATTpitch_deg, ATTheading_deg;
float fixType_f;

uint32_t gpsRate = 50 * 1000; // 20 Hz

void init() {
  gpsLock = xSemaphoreCreateMutex();

  myGNSS.begin(Wire, GPS_I2C_ADDR);
  myGNSS.setPortOutput(COM_PORT_I2C,
                       COM_TYPE_UBX); // Set the I2C port to output UBX only
  myGNSS.saveConfigSelective(
      VAL_CFG_SUBSEC_IOPORT); // Save (only) the comms port settings
  myGNSS.setHNRNavigationRate(30);
  myGNSS.setAutoHNRPVT(true);
  myGNSS.setAutoPVT(true);    // NAV-PVT
  myGNSS.setAutoNAVATT(true); // NAV-ATT
  myGNSS.setAutoNAVSAT(true); // NAV-SAT
  myGNSS.setDynamicModel(
      dynModel::DYN_MODEL_AIRBORNE4g); // ACTIVATE AIRBORNE MODE
  myGNSS.disableDebugging();           // spews config messages
}

void vTaskReadGPS(void *pvParameters) {
  (void)pvParameters;
  Comms::Packet p;
  TickType_t lastWake = xTaskGetTickCount();
  while (1) {
    xSemaphoreTake(gpsLock, portMAX_DELAY);
    bool haveFix = myGNSS.getHNRPVT();
    UBX_HNR_PVT_data_t data = myGNSS.packetUBXHNRPVT->data;
    xSemaphoreGive(gpsLock);

    if (!haveFix) {
      taskDelayUntil(&lastWake, pdMS_TO_TICKS(gpsRate / 1000));
      continue;
    }

    int32_t lat_raw = data.lat;         // deg * 1e-7
    int32_t lon_raw = data.lon;         // deg * 1e-7
    int32_t alt_raw = data.hMSL;        // mm
    uint32_t hAcc_raw = data.hAcc;      // mm
    uint32_t vAcc_raw = data.vAcc;      // mm
    int32_t heading_raw = data.headMot; // deg * 1e-5
    uint32_t hdgAcc_raw = data.headAcc; // deg * 1e-5
    uint8_t fixType = data.gpsFix;

    latitude_deg = lat_raw * 1e-7f;
    longitude_deg = lon_raw * 1e-7f;
    altitude_m = alt_raw * 0.001f;
    heading_deg = heading_raw * 1e-5f;

    ESP_LOGI(TAG, "GPS Lat: %.7f Lon: %.7f Alt: %.3f", latitude_deg,
             longitude_deg, altitude_m);

    PacketGPSValues::Builder()
        .withLatitude(latitude_deg)
        .withLongitude(longitude_deg)
        .withAltitude(altitude_m)
        .withHorizontalAccuracy(hAcc_raw)
        .withVerticalAccuracy(vAcc_raw)
        .withHeading(heading_deg)
        .withHeadingAccuracy(hdgAcc_raw)
        .withFixType(fixType)
        .withSiv(SIV_f)
        .build()
        .writeRawPacket(&p);

    Comms::emitPacketOverAllInterfaces(&p);
    taskDelayUntil(&lastWake, pdMS_TO_TICKS(gpsRate / 1000));
  }
}

void vTaskReadGPSExtra(void *pvParameters) {
  (void)pvParameters;
  Comms::Packet p;
  TickType_t lastWake = xTaskGetTickCount();
  while (1) {
    xSemaphoreTake(gpsLock, portMAX_DELAY);
    bool haveFix = myGNSS.getPVT() && myGNSS.getNAVATT();
    UBX_NAV_PVT_data_t data = myGNSS.packetUBXNAVPVT->data;
    UBX_NAV_ATT_data_t attData = myGNSS.packetUBXNAVATT->data;
    xSemaphoreGive(gpsLock);

    if (!haveFix) {
      taskDelayUntil(&lastWake, pdMS_TO_TICKS(500));
      continue;
    }

    SIV_f = data.numSV;
    PacketGPSExtraValues::Builder()
        .withPositionAccuracy(data.pDOP * 0.01f)
        .withGroundSpeed(data.gSpeed * 0.001f)
        .withATTroll(attData.roll * 1e-5f)
        .withATTpitch(attData.pitch * 1e-5f)
        .withATTheading(attData.heading * 1e-5f)
        .build()
        .writeRawPacket(&p);

    Comms::emitPacketOverAllInterfaces(&p);
    taskDelayUntil(&lastWake, pdMS_TO_TICKS(500));
  }
}

void vTaskReadGPSSatInfo(void *pvParameters) {
  (void)pvParameters;
  Comms::Packet p;
  constexpr uint8_t max = 20; // max amount of satellites to send data about
  TickType_t lastWake = xTaskGetTickCount();
  while (1) {
    std::array<uint8_t, max> gnssID{};
    std::array<uint8_t, max> svID{};
    std::array<uint8_t, max> cno{};
    uint8_t siv = 0;
    bool haveFix;

    xSemaphoreTake(gpsLock, portMAX_DELAY);
    haveFix = myGNSS.getNAVSAT();
    if (haveFix) {
      // Copy the satellite blocks out while we still hold the lock: the
      // UBX_NAV_SAT_data_t struct only points into the library's buffer.
      UBX_NAV_SAT_data_t data = myGNSS.packetUBXNAVSAT->data;
      siv = data.header.numSvs;
      UBX_NAV_SAT_block_t *blocks = data.blocks;
      for (uint8_t i = 0; (i < siv) && (i < max); i++) {
        UBX_NAV_SAT_block_t sat = blocks[i];
        gnssID[i] = sat.gnssId;
        svID[i] = sat.svId;
        cno[i] = sat.cno;
      }
    }
    xSemaphoreGive(gpsLock);

    if (!haveFix) {
      taskDelayUntil(&lastWake, pdMS_TO_TICKS(500));
      continue;
    }

    PacketGPSSatInfo::Builder()
        .withSiv(siv)
        .withGnssID(gnssID)
        .withSvID(svID)
        .withCno(cno)
        .build()
        .writeRawPacket(&p);

    Comms::emitPacketOverAllInterfaces(&p);
    taskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));
  }
}
} // namespace GPS
