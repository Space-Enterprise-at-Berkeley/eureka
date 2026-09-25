#include "Radio.h"
#include "USBComms.h"
#include "proto/Packet_FCEnableTelemetryRadio.h"
#include "proto/Packet_FCEnterP900ConfigMode.h"
namespace RadioComms {
  // internal time tracking
  static uint32_t startTime = 0;
  
  // UART config for the P400 radio
#define RADIO_BAUD 19200
#define RADIO_TX 10
#define RADIO_RX 9

#define RADIO_POWER_PIN 21

  // internal enabled flags
  static bool radioEnabled = false;     // logical enabled state
  static bool radioWaiting = false;      // true if waiting for radio to turn on

  bool isRadioEnabled() {
    return radioEnabled || radioWaiting;
  }

  void enableRadio() {
    // If already enabled, no-op
    if (radioEnabled || radioWaiting) return;
    // Power on sequence for P900
    digitalWrite(RADIO_POWER_PIN, HIGH);
    // Give time for power rail and radio firmware to stabilize before UART ready.
    startTime = micros();
    //did serial start?
    Serial.write("yay Radio enabled");

    radioEnabled = false;
    radioWaiting = true;
  }
  
  void disableRadio() {
    digitalWrite(RADIO_POWER_PIN, LOW);
    Serial.write("oh no Radio disabled");
    radioEnabled = false;
  }

  void radio_en_callback(Comms::Packet packet, uint8_t ip) {
    Serial.println("Radio callback invoked");
    bool en = (bool) Comms::packetGetUint8(&packet, 0);
    if (en) {
      enableRadio();
    } else {
      disableRadio();
    }
  }

  /*
  CFC becomes a serial passthrough to the P900, until told to exit config mode, 
  or 2 minutes have passed with no commands. No other CFC tasks run during this time.
  Escape sequence is "===" sent over main Serial, or a packet with disable command.
  */
  bool radioInConfigMode = false;
  uint32_t radioConfigModeTimeout = 2 * 60 * 1000 * 1000; // 2 minutes in microseconds
  uint32_t lastActivityTime = 0;
  uint8_t escape_index = 0;
  void radioConfigModeCallback(Comms::Packet packet, uint8_t ip) {

    Serial.println("Radio config mode callback invoked");
    bool cmd = (bool) Comms::packetGetUint8(&packet, 0);
    if (cmd && radioInConfigMode) {
      Serial.println("Already in radio config mode");
      return;
    }
    radioInConfigMode = cmd;
    lastActivityTime = micros();


    while (radioInConfigMode) {
      // Check for timeout
      if (micros() - lastActivityTime > radioConfigModeTimeout) {
        Serial.println("Exiting radio config mode due to timeout");
        radioInConfigMode = false;
        break;
      }
      // Check for incoming data from Serial1 (P900)
      while (Serial1.available()) {
        char c = Serial1.read();
        Serial.write(c); // Echo to main Serial
        lastActivityTime = micros();
      }
      // Check for incoming data from main Serial
      while (Serial.available()) {
        char c = Serial.read();
        Serial1.write(c); // Send to P900
        lastActivityTime = micros();
        // Check for escape sequence
        if (c == '=') {
          escape_index++;
          if (escape_index >= 3) {
            Serial.println("Exiting radio config mode via escape sequence");
            radioInConfigMode = false;
            break;
          }
        } else {
          escape_index = 0; // Reset if mismatch
        }
      }
      USBComms::processWaitingPackets(); // Keep processing other packets for disable command
    }
  } 

  void init() {
    // keep radio powered off at startup
    // pinMode(RADIO_POWER_PIN, OUTPUT);
    // digitalWrite(RADIO_POWER_PIN, LOW); // ensure P900 is off at startup
    radioEnabled = true;
    radioWaiting = false;

    Serial1.begin(RADIO_BAUD, SERIAL_8N1, RADIO_RX, RADIO_TX, false);  //starts serial
    //Comms::registerCallback(PACKET_ID_FCEnableTelemetryRadio, radio_en_callback);
    Comms::registerCallback(PACKET_ID_FCEnterP900ConfigMode, radioConfigModeCallback);
  }

static constexpr uint8_t opens[2] = {'[', '['};
static constexpr uint8_t closes[2] = {']', ']'};
  void emitPacket(Comms::Packet *packet) {

    // Send the packet over the radio
    //Comms::finishPacket(packet); //handles in Comms.cpp
    //Emit over Serial
    if (!radioEnabled) {
      if (radioWaiting) {
        if (micros() - startTime > 5 * 1000 * 1000) {
          radioEnabled = true;
          radioWaiting = false;
          task_transmitCallsign(); // send callsign immediately
        }
      }
      return;
    }

    Serial1.write(opens, 2);
    Serial1.write(packet->id);
    Serial1.write(packet->len);
    Serial1.write((uint8_t *)&packet->timestamp, 4);
    Serial1.write((uint8_t *)&packet->checksum, 2);
    Serial1.write(packet->data, packet->len);
    Serial1.write(closes, 2);
  }

  uint32_t testRadioTransmit() {
    Serial1.write("my name sohom roy");
    return 500 * 1000;
  }

  uint32_t task_transmitCallsign() {
    Serial1.write("NU6XB/FVT1");
    return 10 * 60 * 1000 * 1000; // Transmit every 10 minutes
  }

      // Header layout on wire (8 bytes)
  // 0:id, 1:len, 2..5:ts, 6:csum_low, 7:csum_high
  constexpr size_t HEADER_LEN = 8;
  constexpr size_t TRAILER_LEN = 2; // "]]"
  constexpr size_t PACKET_META = 1 + 1 + 4 + 2; // id+len+ts+csum (8)

  // Compute max payload from the struct size
  constexpr size_t MAX_PAYLOAD = sizeof(((Comms::Packet*)0)->data);

  enum class State : uint8_t { SYNC0, SYNC1, READ_HDR, READ_DATA, READ_TRAIL };
  static State state = State::SYNC0;

  static uint8_t hdr[HEADER_LEN];
  static size_t  hdr_pos = 0;

  static uint8_t payload[(MAX_PAYLOAD > 0 ? MAX_PAYLOAD : 1)];
  static size_t  data_pos = 0;

  static uint8_t trailer_pos = 0; // 0 -> expecting first ']', 1 -> expecting second ']'

  char packetBuffer[sizeof(Comms::Packet)];

  void processWaitingPackets() {

    while (Serial1.available() > 0) {
      char c = Serial1.read();
      //if (c < 0) break; // safety? whats that

      switch (state) {
        case State::SYNC0: {
          if (c == '[') state = State::SYNC1;
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

              //this entire chunk could be discarded 
              // Complete candidate frame acquired; verify and dispatch
              auto *packet = reinterpret_cast<Comms::Packet*>(packetBuffer);

              // Fill struct fields (matches how emitPacket writes them)
              packet->id  = hdr[0];
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
                Serial.println("parsed serial packet");
                Comms::evokeCallbackFunction(packet, 0);
              }

              // Ready for the next frame
              state = State::SYNC0;
            }
          }
        } break;
      } // switch
    }   // while available
  }
  
    
};
