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
#include <Comms.h>
#include <USBComms.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_PIN 41
#define PDB_ENABLE_PIN 26

#define SPI_MOSI_PIN 15
#define SPI_MISO_PIN 16
#define SPI_SCLK_PIN 17

#define I2C_SDA_PIN 1
#define I2C_SCL_PIN 2

static const char *TAG = "main";

bool pinstate = 0;
static void prvHelloWorldTask(void *pvParameters) {
  while (1) {
    ESP_LOGI(TAG, "my name sohom roy");
    gpio_set_level(LED_PIN, pinstate);
    pinstate = !pinstate;
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

volatile bool runcam_PDB_enabled = false;
void runcam_PDB_enable_cb(Comms::Packet packet, uint8_t ip) {
  ESP_LOGI(TAG, "Received PDB enable command");
  if (Comms::packetGetUint8(&packet, 0)) {
    gpio_set_level(PDB_ENABLE_PIN, 1);
    runcam_PDB_enabled = true;
    ESP_LOGI(TAG, "PDB Enabled");
    ESP_LOGI(TAG, "Runcam: %d", runcam_PDB_enabled ? 1 : 0);
  } else {
    gpio_set_level(PDB_ENABLE_PIN, 0);
    runcam_PDB_enabled = false;
    ESP_LOGI(TAG, "PDB Disabled");
  }
}

uint32_t avgTaskDelay = 0;
uint32_t avgTaskCounter = 0;
Comms::Packet p;
static void prvSendCFCHealthTask(void *pvParameters) {
  (void)pvParameters;
  while (1) {
    ESP_LOGI(TAG, "Runcam: %d", runcam_PDB_enabled ? 1 : 0);
    PacketFCHealth::Builder()
        .withRuncamEnabled(runcam_PDB_enabled ? 1 : 0)
        .withAvgTaskDelay(avgTaskCounter == 0 ? 0
                                              : avgTaskDelay / avgTaskCounter)
        .withBlackboxWritePointer(Blackbox::getAddr())
        .withRadioEnabled(RadioComms::isRadioEnabled() ? 1 : 0)
        .withBlackboxEnabled(Blackbox::getEnable() ? 1 : 0)
        .build()
        .writeRawPacket(&p);
    Comms::emitPacketOverAllInterfaces(&p);
    avgTaskDelay = 0;
    avgTaskCounter = 0;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

Task taskTable[] = {
    {task_helloWorld, 0, true},
    {Power::task_readSendPower, 0, true},
    {Barometer::sampleBaro, 0, true},
    {Barometer::averageBaro, 0, true},
    {RadioComms::task_transmitCallsign, 0, true},
    {TempSense::task_readSendTemp, 0, true},
    {IMU::task_lowIMUsend, 0, true},
    {IMU::task_highIMUsend, 0, true},
    {GPS::task_readGPS, 0, true},
    {GPS::task_readGPSExtra, 0, true},
    {GPS::task_readGPSSatInfo, 0, true},
    {task_sendCFCHealth, 0, true},
};

#define TASK_COUNT (sizeof(taskTable) / sizeof(struct Task))

/**
 * @brief Initializes the SPI2 (FSPI) bus used by the sensor suite.
 *
 * Uses the ESP-IDF SPI master driver instead of the Arduino SPIClass.
 * FreeRTOS itself has no SPI API; SPI is an ESP-IDF peripheral driver.
 */
static esp_err_t prvSetupSPI(void) {
  spi_bus_config_t buscfg = {};
  buscfg.mosi_io_num = SPI_MOSI_PIN;
  buscfg.miso_io_num = SPI_MISO_PIN;
  buscfg.sclk_io_num = SPI_SCLK_PIN;
  buscfg.quadwp_io_num = -1;
  buscfg.quadhd_io_num = -1;
  buscfg.max_transfer_sz = 4096;

  return spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
}

/**
 * @brief Initializes the I2C master bus used by the sensor suite.
 *
 * Uses the ESP-IDF I2C driver instead of the Arduino TwoWire class.
 * FreeRTOS itself has no I2C API; I2C is an ESP-IDF peripheral driver.
 *
 * Internal pullups are disabled because the board provides external ones.
 */
static esp_err_t prvSetupI2C(void) {
  i2c_config_t conf = {};
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = I2C_SDA_PIN;
  conf.scl_io_num = I2C_SCL_PIN;
  conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
  conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
  conf.master.clk_speed = 400000;
  conf.clk_flags = 0;

  esp_err_t err = i2c_param_config(I2C_NUM_0, &conf);
  if (err != ESP_OK) {
    return err;
  }
  return i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
}

/**
 * @brief Configures the output GPIOs (status LED, PDB enable) via the
 * ESP-IDF GPIO driver instead of Arduino pinMode/digitalWrite.
 */
static void prvSetupGPIO(void) {
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = (1ULL << LED_PIN) | (1ULL << PDB_ENABLE_PIN);
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  ESP_ERROR_CHECK(gpio_config(&io_conf));

  // ensure PDB is off at startup
  ESP_ERROR_CHECK(gpio_set_level(PDB_ENABLE_PIN, 0));
}

static void prvSetupHardware() {
  ESP_ERROR_CHECK(prvSetupSPI());
  ESP_ERROR_CHECK(prvSetupI2C());
  prvSetupGPIO();
  USBComms::init();
  RadioComms::init();
  Power::init();
  Barometer::init();
  TempSense::init();
  IMU::init_lowIMU();
  IMU::init_highIMU();
  Comms::registerCallback(PACKET_ID_FCEnableRuncamPDB, runcam_PDB_enable_cb);
  runcam_PDB_enabled = false;
  GPS::init();
  Blackbox::init();
}

extern "C" void app_main(void) {
  prvSetupHardware();
  xTaskCreate(prvHelloWorldTask, "hello_world", 2048, NULL, 1, NULL);
  xTaskCreate(prvSendCFCHealthTask, "cfc_health", 4096, NULL, 1, NULL);
  // TODO: create one task per module task function once each is converted
  // to a FreeRTOS task body (Power, Barometer, Radio, TempSense, IMU, GPS).
}
