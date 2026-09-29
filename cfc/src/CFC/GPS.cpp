#include "GPS.h"
#include <SPI.h>
#include <array>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <u-blox_structs.h>
#include "proto/Packet_GPSValues.h"
#include "proto/Packet_GPSExtraValues.h"
#include "proto/Packet_GPSSatInfo.h"

#define GPS_I2C_ADDR 0x42

SFE_UBLOX_GNSS myGNSS;

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

Comms::Packet p;

void init() {
  myGNSS.begin(Wire, GPS_I2C_ADDR);
  myGNSS.setPortOutput(COM_PORT_I2C, COM_TYPE_UBX); //Set the SPI port to output UBX only (turn off NMEA noise)
  myGNSS.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT); //Save (only) the communications port settings to flash and BBR
  myGNSS.setHNRNavigationRate(30);
  myGNSS.setAutoHNRPVT(true);
  myGNSS.setAutoPVT(true); // NAV-PVT
  myGNSS.setAutoNAVATT(true); // NAV-ATT
  myGNSS.setAutoNAVSAT(true); // NAV-SAT
  myGNSS.setDynamicModel(dynModel::DYN_MODEL_AIRBORNE4g); // ACTIVATE AIRBORNE MODE
  myGNSS.disableDebugging(); //spews config messages
}

uint32_t task_readGPS() {
  if (!myGNSS.getHNRPVT()) {
    return gpsRate; // try again later
  }
  UBX_HNR_PVT_data_t data = myGNSS.packetUBXHNRPVT->data; 
 
  int32_t lat_raw       = data.lat;                      // deg * 1e-7
  int32_t lon_raw       = data.lon;                      // deg * 1e-7
  int32_t alt_raw       = data.hMSL;                     // mm
  //uint8_t SIV           = 0;//myGNSS.getSIV();
  uint32_t hAcc_raw     = data.hAcc;                     // mm
  uint32_t vAcc_raw     = data.vAcc;                     // mm
  //uint32_t pAcc_raw     = 0;//myGNSS.getPositionAccuracy();  // mm
  int32_t heading_raw   = data.headMot;                  // deg * 1e-5
  uint32_t hdgAcc_raw   = data.headAcc;                  // deg * 1e-5
  uint8_t fixType       = data.gpsFix;
  //int32_t attRoll_raw   = 0;//myGNSS.getATTroll();           // deg * 1e-5
  //int32_t attPitch_raw  = 0;//myGNSS.getATTpitch();          // deg * 1e-5
  //int32_t attHead_raw   = 0;//myGNSS.getATTheading();        // deg * 1e-5
 
  latitude_deg        = lat_raw      * 1e-7f;
  longitude_deg       = lon_raw      * 1e-7f;
  altitude_m          = alt_raw      * 0.001f;
  heading_deg         = heading_raw  * 1e-5f;
  //ATTroll_deg         = attRoll_raw  * 1e-5f;
  //ATTpitch_deg        = attPitch_raw * 1e-5f;
  //ATTheading_deg      = attHead_raw  * 1e-5f;

  Serial.print("GPS Lat: "); Serial.print(latitude_deg, 7);
  Serial.print(" Lon: "); Serial.print(longitude_deg, 7);
  Serial.print(" Alt: "); Serial.print(altitude_m, 3);

  PacketGPSValues::Builder()
    .withLatitude(latitude_deg)
    .withLongitude(longitude_deg) 
    .withAltitude(altitude_m)
    //.withSiv(SIV)
    .withHorizontalAccuracy(hAcc_raw)
    .withVerticalAccuracy(vAcc_raw)
    //.withPositionAccuracy(pAcc_raw)
    .withHeading(heading_deg)
    .withHeadingAccuracy(hdgAcc_raw)
    .withFixType(fixType)
    .withSiv(SIV_f)
    //.withATTroll(ATTroll_deg)
    //.withATTpitch(ATTpitch_deg)
    //.withATTheading(ATTheading_deg)
    .build()
    .writeRawPacket(&p);
  // 54 bytes total, including header

  Comms::emitPacketOverAllInterfaces(&p);
  return gpsRate; // 30 Hz
}

uint32_t task_readGPSExtra() {
  if (!myGNSS.getPVT() || !myGNSS.getNAVATT()) {
    return 500*1000; // try again later
  }
  UBX_NAV_PVT_data_t data = myGNSS.packetUBXNAVPVT->data;
  UBX_NAV_ATT_data_t attData = myGNSS.packetUBXNAVATT->data;
  SIV_f = data.numSV;
  PacketGPSExtraValues::Builder()
    //.withSiv(SIV_f)
    .withPositionAccuracy(data.pDOP * 0.01f)
    .withGroundSpeed(data.gSpeed * 0.001f)
    .withATTroll(attData.roll * 1e-5f)
    .withATTpitch(attData.pitch * 1e-5f)
    .withATTheading(attData.heading * 1e-5f)
    .build()
    .writeRawPacket(&p);
  
  Comms::emitPacketOverAllInterfaces(&p);
  return 500*1000; // 2 Hz
}

uint32_t task_readGPSSatInfo() {
  if (!myGNSS.getNAVSAT()) {
    return 500*1000; // try again later
  }
  UBX_NAV_SAT_data_t data = myGNSS.packetUBXNAVSAT->data;
  uint8_t siv = data.header.numSvs;
  UBX_NAV_SAT_block_t *blocks = data.blocks;

  constexpr uint8_t max = 20; // max amount of satellites to send data about

  std::array<uint8_t, max> gnssID{};
  std::array<uint8_t, max> svID{};
  std::array<uint8_t, max> cno{};

  for (uint8_t i = 0; (i < siv) && (i < max); i++) {
    UBX_NAV_SAT_block_t sat = blocks[i];
    gnssID[i] = sat.gnssId;
    svID[i] = sat.svId;
    cno[i] = sat.cno;
  }

  PacketGPSSatInfo::Builder()
    .withSiv(siv)
    .withGnssID(gnssID)
    .withSvID(svID)
    .withCno(cno)
    .build()
    .writeRawPacket(&p);

    Comms::emitPacketOverAllInterfaces(&p);
    return 1000*1000; // 1 Hz
}

} // namespace GPS
