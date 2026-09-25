#include "Radio.h"
#include "USBComms.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "proto/Packet_FCEnableTelemetryRadio.h"
#include "proto/Packet_FCEnterP900ConfigMode.h"

static const char *TAG = "radio";

namespace RadioComms {
// internal time tracking
static int64_t startTime = 0;

// UART config for the P400 radio
#define RADIO_BAUD 19200
#define RADIO_TX 10
#define RADIO_RX 9

#define RADIO_POWER_PIN 21
#define RADIO_CALLSIGN "NU6XB/FVT1"

// internal enabled flags
static bool radioEnabled = false; // logical enabled state
static bool radioWaiting = false; // true if waiting for radio to turn on

bool isRadioEnabled() { return radioEnabled || radioWaiting; }

void enableRadio() {
  // If already enabled, no-op
  if (radioEnabled || radioWaiting)
    return;
  // Power on sequence for P900
  gpio_set_level(RADIO_POWER_PIN, 1);
  // Give time for power rail and radio firmware to stabilize before UART ready.
  startTime = esp_timer_get_time();
  // did serial start?
  ESP_LOGI(TAG, "yay Radio enabled");

  radioEnabled = false;
  radioWaiting = true;
}

void disableRadio() {
  gpio_set_level(RADIO_POWER_PIN, 0);
  ESP_LOGI(TAG, "oh no Radio disabled");
  radioEnabled = false;
}

void radio_en_callback(Comms::Packet packet, uint8_t ip) {
  ESP_LOGI(TAG, "Radio callback invoked");
  bool en = (bool)Comms::packetGetUint8(&packet, 0);
  if (en) {
    enableRadio();
  } else {
    disableRadio();
  }
}

/*
CFC becomes a serial passthrough to the P900, until told to exit config mode,
or 2 minutes have passed with no commands. No other CFC tasks run during this
time. Escape sequence is "===" sent over main Serial, or a packet with disable
command.
*/
bool radioInConfigMode = false;
int64_t radioConfigModeTimeout =
    2 * 60 * 1000 * 1000; // 2 minutes in microseconds
int64_t lastActivityTime = 0;
uint8_t escape_index = 0;
void radioConfigModeCallback(Comms::Packet packet, uint8_t ip) {

  ESP_LOGI(TAG, "Radio config mode callback invoked");
  bool cmd = (bool)Comms::packetGetUint8(&packet, 0);
  if (cmd && radioInConfigMode) {
    ESP_LOGI(TAG, "Already in radio config mode");
    return;
  }
  radioInConfigMode = cmd;
  lastActivityTime = esp_timer_get_time();

  while (radioInConfigMode) {
    // Check for timeout
    if (esp_timer_get_time() - lastActivityTime > radioConfigModeTimeout) {
      ESP_LOGI(TAG, "Exiting radio config mode due to timeout");
      radioInConfigMode = false;
      break;
    }
    // Check for incoming data from the P900 UART
    uint8_t c;
    while (uart_read_bytes(UART_NUM_1, &c, 1, 0) == 1) {
      usb_serial_jtag_write_bytes(&c, 1, portMAX_DELAY); // Echo to main USB
      lastActivityTime = esp_timer_get_time();
    }
    // Check for incoming data from main USB
    while (usb_serial_jtag_read_bytes(&c, 1, 0) == 1) {
      uart_write_bytes(UART_NUM_1, &c, 1); // Send to P900
      lastActivityTime = esp_timer_get_time();
      // Check for escape sequence
      if (c == '=') {
        escape_index++;
        if (escape_index >= 3) {
          ESP_LOGI(TAG, "Exiting radio config mode via escape sequence");
          radioInConfigMode = false;
          break;
        }
      } else {
        escape_index = 0; // Reset if mismatch
      }
    }
    USBComms::processWaitingPackets(); // Keep processing other packets for
                                       // disable command
  }
}

void init() {
  radioEnabled = true;
  radioWaiting = false;

  uart_config_t uart_config = {};
  uart_config.baud_rate = RADIO_BAUD;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.source_clk = UART_SCLK_APB;

  ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, RADIO_TX, RADIO_RX,
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 256, 256, 0, NULL, 0));

  Comms::registerCallback(PACKET_ID_FCEnterP900ConfigMode,
                          radioConfigModeCallback);
}

static void sendCallsign() {
  uart_write_bytes(UART_NUM_1, RADIO_CALLSIGN, sizeof(RADIO_CALLSIGN) - 1);
}

static constexpr uint8_t opens[2] = {'[', '['};
static constexpr uint8_t closes[2] = {']', ']'};
void emitPacket(Comms::Packet *packet) {

  // Send the packet over the radio
  // Comms::finishPacket(packet); //handles in Comms.cpp
  // Emit over the radio UART
  if (!radioEnabled) {
    if (radioWaiting) {
      if (esp_timer_get_time() - startTime > 5 * 1000 * 1000) {
        radioEnabled = true;
        radioWaiting = false;
        sendCallsign(); // send callsign immediately
      }
    }
    return;
  }

  uart_write_bytes(UART_NUM_1, opens, sizeof(opens));
  uart_write_bytes(UART_NUM_1, &packet->id, sizeof(packet->id));
  uart_write_bytes(UART_NUM_1, &packet->len, sizeof(packet->len));
  uart_write_bytes(UART_NUM_1, packet->timestamp, sizeof(packet->timestamp));
  uart_write_bytes(UART_NUM_1, packet->checksum, sizeof(packet->checksum));
  uart_write_bytes(UART_NUM_1, packet->data, packet->len);
  uart_write_bytes(UART_NUM_1, closes, sizeof(closes));
}

void vTaskTestRadioTransmit(void *pvParameters) {
  (void)pvParameters;
  while (1) {
    uart_write_bytes(UART_NUM_1, "my name sohom roy",
                     sizeof("my name sohom roy") - 1);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void vTaskTransmitCallsign(void *pvParameters) {
  (void)pvParameters;
  while (1) {
    sendCallsign();
    vTaskDelay(pdMS_TO_TICKS(10 * 60 * 1000)); // Transmit every 10 minutes
  }
}

// Header layout on wire (8 bytes)
// 0:id, 1:len, 2..5:ts, 6:csum_low, 7:csum_high
constexpr size_t HEADER_LEN = 8;
constexpr size_t TRAILER_LEN = 2;             // "]]"
constexpr size_t PACKET_META = 1 + 1 + 4 + 2; // id+len+ts+csum (8)

// Compute max payload from the struct size
constexpr size_t MAX_PAYLOAD = sizeof(((Comms::Packet *)0)->data);

enum class State : uint8_t { SYNC0, SYNC1, READ_HDR, READ_DATA, READ_TRAIL };
static State state = State::SYNC0;

static uint8_t hdr[HEADER_LEN];
static size_t hdr_pos = 0;

static uint8_t payload[(MAX_PAYLOAD > 0 ? MAX_PAYLOAD : 1)];
static size_t data_pos = 0;

static uint8_t trailer_pos =
    0; // 0 -> expecting first ']', 1 -> expecting second ']'

char packetBuffer[sizeof(Comms::Packet)];

void processWaitingPackets() {

  uint8_t c;
  while (uart_read_bytes(UART_NUM_1, &c, 1, 0) == 1) {

    switch (state) {
    case State::SYNC0: {
      if (c == '[')
        state = State::SYNC1;
      // else keep hunting
    } break;

    case State::SYNC1: {
      if (c == '[') {
        // Found "[["
        hdr_pos = 0;
        state = State::READ_HDR;
      } else {
        // If we see another '[', stay in SYNC1; else restart SYNC
        state = (c == '[') ? State::SYNC1 : State::SYNC0;
      }
    } break;

    case State::READ_HDR: {
      hdr[hdr_pos++] = c;
      if (hdr_pos == HEADER_LEN) {
        uint8_t len = hdr[1];
        if (len > MAX_PAYLOAD) {
          // Impossible/too large -> bad packet
          state = State::SYNC0;
        } else {
          data_pos = 0;
          state = (len == 0) ? State::READ_TRAIL : State::READ_DATA;
          trailer_pos = 0;
        }
      }
    } break;

    case State::READ_DATA: {
      // Collect exactly len bytes
      payload[data_pos++] = c;
      if (data_pos == hdr[1]) {
        trailer_pos = 0;
        state = State::READ_TRAIL;
      }

    } break;

    case State::READ_TRAIL: {
      // Expect two consecutive ']'
      if (trailer_pos == 0) {
        if (c != ']') {
          state = State::SYNC0;
        } else {
          trailer_pos = 1;
        }
      } else { // trailer_pos == 1
        if (c != ']') {
          state = State::SYNC0;
        } else {

          // this entire chunk could be discarded
          //  Complete candidate frame acquired; verify and dispatch
          auto *packet = reinterpret_cast<Comms::Packet *>(packetBuffer);

          // Fill struct fields (matches how emitPacket writes them)
          packet->id = hdr[0];
          packet->len = hdr[1];

          // Timestamp (4 bytes as stored on wire)
          std::memcpy(&(packet->timestamp), &hdr[2], 4);

          // Checksum (wire order: low, high)
          packet->checksum[0] = hdr[6];
          packet->checksum[1] = hdr[7];

          // Payload
          if (packet->len > 0) {
            std::memcpy(packet->data, payload, packet->len);
          }

          // Validate checksum against content (id,len,ts,data)
          const uint16_t wire =
              static_cast<uint16_t>(packet->checksum[0]) |
              (static_cast<uint16_t>(packet->checksum[1]) << 8);
          const uint16_t calc = Comms::computePacketChecksum(packet);

          if (calc != wire) {
          } else {
            // Good packet: dispatch (0 signifies a USB packet)
            ESP_LOGI(TAG, "parsed serial packet");
            Comms::evokeCallbackFunction(packet, 0);
          }

          // Ready for the next frame
          state = State::SYNC0;
        }
      }
    } break;
    } // switch
  } // while available
}

}; // namespace RadioComms
