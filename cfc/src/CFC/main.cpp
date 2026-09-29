#include "Barometer.h"
#include "Blackbox.h"
#include "Common.h"
#include "GPS.h"
#include "IMU.h"
#include "Power.h"
#include "Radio.h"
#include "TempSense.h"
#include "proto/Packet_DummyData100ms.h"
#include "proto/Packet_DummyData10ms.h"
#include "proto/Packet_DummyData1ms.h"
#include "proto/Packet_DummyData1s.h"
#include "proto/Packet_FCEnableRuncamPDB.h"
#include "proto/Packet_FCHealth.h"
#include <Arduino.h>
#include <Comms.h>
#include <SPI.h>
#include <USBComms.h>
#include <Wire.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_PIN 41
#define PDB_ENABLE_PIN 26
#define SPI_SCK 17
#define SPI_MISO 16
#define SPI_MOSI 15
#define I2C_SDA 1
#define I2C_SCL 2

static const char *TAG = "main";

static uint32_t avgTaskDelay = 0;
static uint32_t avgTaskCounter = 0;
static portMUX_TYPE taskDelayMux = portMUX_INITIALIZER_UNLOCKED;

void taskDelayUntil(TickType_t *lastWake, TickType_t period) {
  vTaskDelayUntil(lastWake, period);
  uint32_t lateUs =
      (xTaskGetTickCount() - *lastWake) * portTICK_PERIOD_MS * 1000;
  portENTER_CRITICAL(&taskDelayMux);
  avgTaskDelay += lateUs;
  avgTaskCounter++;
  portEXIT_CRITICAL(&taskDelayMux);
}

bool pinstate = 0;
static void prvHelloWorldTask(void *pvParameters) {
  TickType_t lastWake = xTaskGetTickCount();
  while (1) {
    ESP_LOGI(TAG, "my name sohom roy");
    digitalWrite(LED_PIN, pinstate);
    pinstate = !pinstate;
    taskDelayUntil(&lastWake, pdMS_TO_TICKS(500));
  }
}

volatile bool runcam_PDB_enabled = false;
void runcam_PDB_enable_cb(Comms::Packet packet, uint8_t ip) {
  Serial.println("Received PDB enable command");
  if (Comms::packetGetUint8(&packet, 0)) {
    digitalWrite(PDB_ENABLE_PIN, HIGH);
    runcam_PDB_enabled = true;
    Serial.println("PDB Enabled");
    Serial.print("Runcam: ");
    Serial.println(runcam_PDB_enabled ? 1 : 0);
  } else {
    digitalWrite(PDB_ENABLE_PIN, LOW);
    runcam_PDB_enabled = false;
    Serial.println("PDB Disabled");
  }
}

static void prvProcessWaitingPacketsTask(void *pvParameters) {
  (void)pvParameters;
  while (1) {
    Comms::processWaitingPackets();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

Comms::Packet p;
static void prvSendCFCHealthTask(void *pvParameters) {
  (void)pvParameters;
  TickType_t lastWake = xTaskGetTickCount();
  while (1) {
    portENTER_CRITICAL(&taskDelayMux);
    uint32_t delaySum = avgTaskDelay;
    uint32_t delayCount = avgTaskCounter;
    avgTaskDelay = 0;
    avgTaskCounter = 0;
    portEXIT_CRITICAL(&taskDelayMux);

    ESP_LOGI(TAG, "Runcam: %d", runcam_PDB_enabled ? 1 : 0);
    PacketFCHealth::Builder()
        .withRuncamEnabled(runcam_PDB_enabled ? 1 : 0)
        .withAvgTaskDelay(delayCount == 0 ? 0 : delaySum / delayCount)
        .withBlackboxWritePointer(Blackbox::getAddr())
        .withRadioEnabled(RadioComms::isRadioEnabled() ? 1 : 0)
        .withBlackboxEnabled(Blackbox::getEnable() ? 1 : 0)
        .build()
        .writeRawPacket(&p);
    Comms::emitPacketOverAllInterfaces(&p);
    taskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));
  }
}

void setup() {
  // setup hardware protocols
  pinMode(SPI_SCK, OUTPUT);
  pinMode(SPI_MISO, INPUT);
  pinMode(SPI_MOSI, OUTPUT);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  Wire.begin(I2C_SDA, I2C_SCL);

  // initialize our hardware
  pinMode(LED_PIN, OUTPUT);
  USBComms::init();
  Serial.setDebugOutput(true); // route ESP_LOGx to USB instead of UART0
  RadioComms::init();
  Power::init();
  Barometer::init();
  TempSense::init();
  IMU::init_lowIMU();
  IMU::init_highIMU();
  Comms::registerCallback(PACKET_ID_FCEnableRuncamPDB, runcam_PDB_enable_cb);
  pinMode(PDB_ENABLE_PIN, OUTPUT);
  digitalWrite(PDB_ENABLE_PIN, LOW); // ensure PDB is off at startup
  runcam_PDB_enabled = false;
  GPS::init();
  Blackbox::init();

  // schedule all of our tasks
  xTaskCreate(prvHelloWorldTask, "hello_world", 2048, NULL, 5, NULL);
  xTaskCreate(Power::vTaskReadSendPower, "send_power", 4096, NULL, 5, NULL);
  xTaskCreate(Barometer::vTaskSampleBaro, "sample_baro", 4096, NULL, 5, NULL);
  xTaskCreate(Barometer::vTaskAverageBaro, "average_baro", 4096, NULL, 5, NULL);
  xTaskCreate(RadioComms::vTaskTransmitCallsign, "transmit_callsign", 2048,
              NULL, 5, NULL);
  xTaskCreate(TempSense::vTaskReadSendTemp, "send_temp", 4096, NULL, 5, NULL);
  xTaskCreate(IMU::vTaskLowIMUSend, "send_lowimu", 4096, NULL, 5, NULL);
  xTaskCreate(IMU::vTaskHighIMUSend, "send_highimu", 4096, NULL, 5, NULL);
  xTaskCreate(GPS::vTaskReadGPS, "read_gps", 4096, NULL, 5, NULL);
  xTaskCreate(GPS::vTaskReadGPSExtra, "read_gpsextra", 4096, NULL, 5, NULL);
  xTaskCreate(GPS::vTaskReadGPSSatInfo, "read_gpssatinfo", 8192, NULL, 5, NULL);
  xTaskCreate(prvSendCFCHealthTask, "send_cfchealth", 4096, NULL, 5, NULL);
  xTaskCreate(prvProcessWaitingPacketsTask, "process_packets", 4096, NULL, 5,
              NULL);
}

// setup() runs in the Arduino loop task; delete it so it doesn't spin on core 1
void loop() { vTaskDelete(NULL); }
