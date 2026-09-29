#include "USBComms.h"
#include <cstring>
//#include "proto/Packet_Abort.h" // This can't go in the header or it will cause a circular import of headers
// ^ this kinda breaks testing bc it uses ESPComms, so unless we make a new usb abort file we can't really do that

namespace USBComms {
  char packetBuffer[sizeof(Comms::Packet)];

  // New: track malformed frames
  static uint32_t badPacketCount = 0;
  uint32_t getBadPacketCount() { return badPacketCount; }
  void resetBadPacketCount() { badPacketCount = 0; }

  void init()
  {
    Serial.begin(19200);
  }

  /**
   * @brief Checks checksum of packet and tries to call the associated callback function.
   *
   * @param packet Packet to be processed.
   * @param ip End byte in IP address of sender (255 if usb packet)
   */

  void processWaitingPackets()
  {
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

    while (Serial.available() > 0) {
      char c = Serial.read();
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
                ++badPacketCount; // wrong checksum
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

    // if (Serial.available())
    //   {
    //     //That was for reading full formed packets from the USB serial port
    //     /*
    //     int cnt = 0;
    //     while (Serial.available() && cnt < sizeof(Packet))
    //     {
    //       packetBuffer[cnt] = Serial.read();
    //       cnt++;
    //     }
    //     Packet *packet = (Packet *)&packetBuffer;
    //     // DEBUG("Got unverified packet with ID ");
    //     // DEBUG(packet->id);
    //     // DEBUG('\n');
    //     evokeCallbackFunction(packet, 255); // 255 signifies a USB packet
    //     */
       
    //    //Instead I want to read commands in the form of "id data"
    //    //And then make the packet and trigger the callback

    //     Serial.println("Got a command");
    //     uint8_t id = (uint8_t)Serial.parseInt();
    //     Serial.print("id" + String(id));
    //     if (id == -1) return;
    //     Comms::Packet packet = {.id = id, .len = 0};
    //     while(Serial.available()){
    //       if (Serial.peek() == ' ') Serial.read();
    //       if (Serial.peek() == '\n') {Serial.read(); break;}
    //       //determine datatype of next value
    //       if (Serial.peek() == 'f'){
    //         Serial.read();
    //         float val = Serial.parseFloat();
    //         Serial.print(" float" + String(val));
    //         Comms::packetAddFloat(&packet, val);
    //       }
    //       else if (Serial.peek() == 'i'){
    //         Serial.read();
    //         int val = Serial.parseInt();
    //         Serial.print(" int" + String(val));
    //         Comms::packetAddUint32(&packet, val);
    //       }
    //       else if (Serial.peek() == 's'){
    //         Serial.read();
    //         int val = Serial.parseInt();
    //         Serial.print(" short" + String(val));
    //         Comms::packetAddUint16(&packet, val);
    //       }
    //       else if (Serial.peek() == 'b'){
    //         Serial.read();
    //         int val = Serial.parseInt();
    //         Serial.print(" byte" + String(val));
    //         packetAddUint8(&packet, val);
    //       } else{
    //         Serial.read();
    //       }
    //     }
    //     Serial.println();
    //         // add timestamp to struct
    //     uint32_t timestamp = millis();
    //     packet.timestamp[0] = timestamp & 0xFF;
    //     packet.timestamp[1] = (timestamp >> 8) & 0xFF;
    //     packet.timestamp[2] = (timestamp >> 16) & 0xFF;
    //     packet.timestamp[3] = (timestamp >> 24) & 0xFF;

    //     // calculate and append checksum to struct
    //     uint16_t checksum = Comms::computePacketChecksum(&packet);
    //     packet.checksum[0] = checksum & 0xFF;
    //     packet.checksum[1] = checksum >> 8;
    //     Comms::evokeCallbackFunction(&packet, 0); // 0 signifies a USB packet
    //   }
  }
  void emitPacket(Comms::Packet *packet)
  {
    //Comms::finishPacket(packet); handles in Comms.cpp
    //Emit over Serial
    const uint8_t opens[2] = {'[', '['};
    Serial.write(opens, 2);

        
    Serial.write(packet->id);
    Serial.write(packet->len);
    Serial.write((uint8_t *)&packet->timestamp, 4);
    Serial.write((uint8_t *)&packet->checksum, 2);
    Serial.write(packet->data, packet->len);

        
    const uint8_t closes[2] = {']', ']'};
    Serial.write(closes, 2);
  }

}