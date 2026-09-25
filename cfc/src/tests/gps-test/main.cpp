#include <SPI.h> //Needed for SPI to GNSS
#include <SparkFun_u-blox_GNSS_Arduino_Library.h> //http://librarymanager/All#SparkFun_u-blox_GNSS
SFE_UBLOX_GNSS myGNSS;

// #########################################

// Instantiate an instance of the SPI class. 
// Your configuration may be different, depending on the microcontroller you are using!

//#define spiPort SPI // This is the SPI port on standard Ardino boards. Comment this line if you want to use a different port.

//SPIClass spiPort (HSPI); // This is the default SPI interface on some ESP32 boards. Uncomment this line if you are using ESP32.

// #########################################

//const uint8_t csPin = 7;

// #########################################

long lastTime = 0; //Simple local timer. Limits amount of SPI traffic to u-blox module.

void setup()
{
  Serial.begin(115200);
  while (!Serial); //Wait for user to open terminal
  Serial.println(F("begining NEO-M8U GPS test"));

  Wire.begin(1, 2);

  myGNSS.enableDebugging(); // Uncomment this line to see helpful debug messages on Serial
  // Connect to the u-blox module using SPI port, csPin and speed setting
  // ublox devices generally work up to 5MHz. We'll use 4MHz for this example:
  if (!myGNSS.begin(Wire, 0x42)) 
  {
    Serial.println(F("u-blox GNSS not detected on I2C bus. Please check wiring. Freezing."));
    while (1);
  }
  
  //myGNSS.factoryDefault(); delay(5000); // Uncomment this line to reset the module back to its factory defaults

  myGNSS.setPortOutput(COM_PORT_I2C, COM_TYPE_UBX); //Set the SPI port to output UBX only (turn off NMEA noise)
  myGNSS.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT); //Save (only) the communications port settings to flash and BBR
  myGNSS.disableDebugging(); //spews config messages
}

void loop()
{

  if (millis() - lastTime > 1000)
  {
      int32_t latitude   = myGNSS.getLatitude();
      int32_t longitude  = myGNSS.getLongitude();
      int32_t altitudeMM = myGNSS.getAltitudeMSL();
      uint8_t SIV        = myGNSS.getSIV();
      uint8_t fixType    = myGNSS.getFixType();

      uint32_t heading         = myGNSS.getHeading();
      uint32_t headingAccuracy = myGNSS.getHeadingAccEst();
      uint32_t speedAccuracy   = myGNSS.getSpeedAccEst();

      uint32_t hAcc = myGNSS.getHorizontalAccuracy();
      uint32_t vAcc = myGNSS.getVerticalAccuracy();
      uint32_t pAcc = myGNSS.getPositionAccuracy();

      Serial.print(F("Lat: ")); Serial.print(latitude);  Serial.println(F(" (deg*1e-7)"));
      Serial.print(F("Lon: ")); Serial.print(longitude); Serial.println(F(" (deg*1e-7)"));
      Serial.print(F("Alt: ")); Serial.print(altitudeMM); Serial.println(F(" (mm)"));
      Serial.print(F("SIV: ")); Serial.println(SIV);
      Serial.print(F("Heading: ")); Serial.print(heading); Serial.println(F(" (deg*1e-5)"));
      Serial.print(F("Speed Acc: ")); Serial.print(speedAccuracy); Serial.println(F(" (mm/s, 1σ)"));
      Serial.print(F("Heading Acc: ")); Serial.print(headingAccuracy); Serial.println(F(" (deg*1e-5, 1σ)"));
      Serial.print(F("Horizontal Accuracy: ")); Serial.print(hAcc); Serial.println(F(" (mm)"));
      Serial.print(F("Vertical Accuracy: ")); Serial.print(vAcc); Serial.println(F(" (mm)"));
      Serial.print(F("Position Accuracy: ")); Serial.print(pAcc); Serial.println(F(" (mm, 1σ, 3D)"));
      Serial.print(F("fix type: ")); Serial.println(fixType);
      Serial.println(F("-------------------------"));
        
      

  }
}
