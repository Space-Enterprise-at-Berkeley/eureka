#include "USBComms.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "usb_comms";

namespace USBComms {
char packetBuffer[sizeof(Comms::Packet)];

// New: track malformed frames
static uint32_t badPacketCount = 0;
uint32_t getBadPacketCount() { return badPacketCount; }
void resetBadPacketCount() { badPacketCount = 0; }

void init() {
  usb_serial_jtag_driver_config_t usb_serial_jtag_config = {};
  usb_serial_jtag_config.tx_buffer_size = 256;
  usb_serial_jtag_config.rx_buffer_size = 256;
  ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));
}

/**
 * @brief Checks checksum of packet and tries to call the associated callback
 * function.
 *
 * @param packet Packet to be processed.
 * @param ip End byte in IP address of sender (255 if usb packet)
 */
void processWaitingPackets() {
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

  uint8_t c;
  while (usb_serial_jtag_read_bytes(&c, 1, 0) == 1) {
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
          ++badPacketCount;
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
          ++badPacketCount;
          state = State::SYNC0;
        } else {
          trailer_pos = 1;
        }
      } else { // trailer_pos == 1
        if (c != ']') {
          ++badPacketCount;
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
            ++badPacketCount; // wrong checksum
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
  }
}

void emitPacket(Comms::Packet *packet) {
  // Comms::finishPacket(packet); handles in Comms.cpp
  // Emit over the USB Serial/JTAG driver
  const uint8_t opens[2] = {'[', '['};
  usb_serial_jtag_write_bytes(opens, sizeof(opens), portMAX_DELAY);

  usb_serial_jtag_write_bytes(&packet->id, sizeof(packet->id), portMAX_DELAY);
  usb_serial_jtag_write_bytes(&packet->len, sizeof(packet->len), portMAX_DELAY);
  usb_serial_jtag_write_bytes(packet->timestamp, sizeof(packet->timestamp),
                              portMAX_DELAY);
  usb_serial_jtag_write_bytes(packet->checksum, sizeof(packet->checksum),
                              portMAX_DELAY);
  usb_serial_jtag_write_bytes(packet->data, packet->len, portMAX_DELAY);

  const uint8_t closes[2] = {']', ']'};
  usb_serial_jtag_write_bytes(closes, sizeof(closes), portMAX_DELAY);
}
} // namespace USBComms
