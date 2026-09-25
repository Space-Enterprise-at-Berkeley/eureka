/*
 * Common Flight Computer Serial Diagnostics
 * Just a bunch of simple diagnostic scripts packed into a namespace.
 *
 * ATTENTION: FCDiagnostics doesn't include the requisite setup routines for all the hardware interfaces on the FC.
 * Refer to the original implementation in Flight-Firmware/src/diagnostic for setup
 */

#include "diagnostics.h"




namespace FCDiagnostics {

SPIClass* sensorSPI = NULL;
HardwareSerial* tlmRadioSerial;
HardwareSerial* lvRadioSerial;

void runI2CScan(int argc, char **argv) {
    byte error, address;
    int nDevices;

    Serial.println("Scanning...");

    nDevices = 0;
    for(address = 1; address < 127; address++ )
    {
        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0)
        {
            Serial.print("I2C device found at address 0x");
            if (address<16)
                Serial.print("0");
            Serial.print(address,HEX);
            Serial.println("  !");

            nDevices++;
        }
        else if (error==4)
        {
            Serial.print("Unknown error at address 0x");
            if (address<16)
                Serial.print("0");
            Serial.println(address,HEX);
        }
    }
    if (nDevices == 0)
        Serial.println("No I2C devices found\n");
    else
        Serial.println("done\n");

}


void runTemperatureSensorTest(int argc, char **argv)
{
    Serial.println("Testing Temperature Sensor... ");
    unsigned int data[2];

    Wire.beginTransmission(ADDRESS_TEMPSENSOR);
    Wire.write(0x00);  // Temperature data register -- 0x00
    Wire.endTransmission();

    // Request 2 bytes of data
    Wire.requestFrom(ADDRESS_TEMPSENSOR,1);

    // Read 2 bytes of data
    // temp msb, temp lsb
    if(Wire.available()>0) {
        data[0] = Wire.read();
        data[1] = Wire.read();
    }

    for (int b = 7; b >= 0; b--) {
        Serial.print(bitRead(data[0], b));
    }
    Serial.println("");

    int temp = (data[0] * 256 + (data[1] & 0x80)) / 128;

    if (temp > 255) {
        temp -= 512;
    }

    float cTemp = temp * 0.5;
    float fTemp = cTemp * 1.8 + 32;

    Serial.print("Temperature in Celsius:  ");
    Serial.print(cTemp);
    Serial.println(" C");
    Serial.print("Temperature in Fahrenheit:  ");
    Serial.print(fTemp);
    Serial.println(" F");
}

void runPowerMonitorTest(INA233 &ina)
{
    Serial.print("Bus Voltage: ");
    Serial.println(ina.readBusVoltage());
    Serial.print("Shunt Current: ");
    Serial.println(ina.readCurrent());
    Serial.print("Shunt Voltage: ");
    Serial.println(ina.readShuntVoltage());
    Serial.print("Power: ");
    Serial.println(ina.readPower());
}

void spamSPIBus(int argc, char **argv, SPIClass* sensorSPIbus)
{
    // Default number of bytes if none (or invalid) provided
    int numBytes = 64;
    if (argc > 1) {
        int requested = atoi(argv[1]);
        if (requested > 0) {
            numBytes = requested;
        } else {
            Serial.print(F("Invalid byte count `"));
            Serial.print(argv[1]);
            Serial.println(F("`; using default of 64."));
        }
    }

    // Select device (uses HIGH_ACCEL_CS_PIN here as an example)
    //digitalWrite(HIGH_ACCEL_CS_PIN, LOW);
    //delayMicroseconds(1);
    delay(1);

    sensorSPIbus->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    for (int i = 0; i < numBytes; i++) {
        uint8_t rnd = (uint8_t)random(0, 256);
        sensorSPIbus->transfer(rnd);
        // Optional: tiny delay to observe activity on a logic analyzer
        delayMicroseconds(2);
    }

    // Deselect device
    //digitalWrite(HIGH_ACCEL_CS_PIN, HIGH);
    //sensorSPIbus->end();
    sensorSPIbus->endTransaction();

    Serial.print(F("Screamed "));
    Serial.print(numBytes);
    Serial.println(F(" random bytes into SPI."));
}

int charArrayToInt(char *digits) {
    int num = 0;
    for (int i = 0; digits[i] != '\0'; ++i) {
        if (digits[i] >= '0' && digits[i] <= '9') {
            num = num * 10 + (digits[i] - '0');
        } else
            return 0;
    }
    return num;
}

void setGPIO(int argc, char **argv)
{
    if (argv[2][1] != 's') {
        Serial.print("Setting IO ");
        Serial.print(argv[1]);
        Serial.print(" to ");
        Serial.print(argv[2]);
        Serial.println(" !");
        digitalWrite(charArrayToInt(argv[1]), charArrayToInt(argv[2]));
    }
    Serial.print("GPIO ");
    Serial.print(argv[1]);
    Serial.print(" is now at state");
    Serial.println(digitalRead(charArrayToInt(argv[2])));
}

// --- High-range accelerometer test for H3LIS331DL ---
void runHighAccelTest(int argc, char **argv, SPIClass* sensorSPIbus) {
	
	sensorSPI = sensorSPIbus;
    Serial.println(F("=== H3LIS331DL High-Range Accel Test ==="));

    // 1) Check WHO_AM_I
    digitalWrite(HIGH_ACCEL_CS_PIN, LOW);
    delay(1);
    uint8_t who = h3_readReg(H3_WHO_AM_I_REG);
    digitalWrite(HIGH_ACCEL_CS_PIN, HIGH);
    if (who != H3_WHO_AM_I_VAL) {
        Serial.print(F("ERROR: unexpected WHO_AM_I = 0x"));
        Serial.println(who, HEX);
        return;
    }
    Serial.println(F("WHO_AM_I correct (0x32)"));

    // 2) Power on + enable X, Y, Z axes (100 Hz ODR)
    digitalWrite(HIGH_ACCEL_CS_PIN, LOW);
    delay(1);
    h3_writeReg(H3_CTRL_REG1, 0x27);  // PD=1, ODR=00 (100 Hz), Zen/Yen/Xen=1
    digitalWrite(HIGH_ACCEL_CS_PIN, HIGH);
    delay(100);

    // 3) Read raw X, Y, Z
    digitalWrite(HIGH_ACCEL_CS_PIN, LOW);
    int16_t x = h3_read16(H3_OUT_X_L);
    int16_t y = h3_read16(H3_OUT_Y_L);
    int16_t z = h3_read16(H3_OUT_Z_L);
    digitalWrite(HIGH_ACCEL_CS_PIN, HIGH);

    // 4) Report results
    Serial.print(F("Raw X: ")); Serial.println(x);
    Serial.print(F("Raw Y: ")); Serial.println(y);
    Serial.print(F("Raw Z: ")); Serial.println(z);
    Serial.println(F("=== Test Complete ==="));
}

// Low-level SPI register I/O for H3LIS331DL
static uint8_t h3_readReg(uint8_t reg) {
    // CS must already be LOW
    sensorSPI->transfer(reg | 0x80);      // R/W=1, MB=0
    return sensorSPI->transfer(0x00);
}

static void h3_writeReg(uint8_t reg, uint8_t val) {
    // CS must already be LOW
    sensorSPI->transfer(reg & 0x7F);      // R/W=0, MB=0
    sensorSPI->transfer(val);
}

// Read a signed 16-bit value from low/high pair
static int16_t h3_read16(uint8_t addr_l) {
    uint8_t lo = h3_readReg(addr_l);
    uint8_t hi = h3_readReg(addr_l + 1);
    return (int16_t)((hi << 8) | lo);
}

void setSerial(HardwareSerial* tlmSerial, HardwareSerial* lvSerial) {
	tlmRadioSerial = tlmSerial;
	lvRadioSerial = lvSerial;
}

void setSensorSPI(SPIClass* input) {
	sensorSPI = input;	
}

void getTlmRadioVersionInConfig(int argc, char **argv) {
	tlmRadioSerial->write("AT&V\n");
	Serial.println("Queried radio: AT&V");
}

void runBarometerTest(int argc, char **argv) {
    Serial.println(F("=== MS5607 Barometer Test ==="));

    // Reset the sensor
    sensorSPI->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
    digitalWrite(BARO_CS_PIN, LOW);
    sensorSPI->transfer(0x1E);  // RESET command
    digitalWrite(BARO_CS_PIN, HIGH);
    sensorSPI->endTransaction();
    delay(10);

    // Read calibration coefficients (C1 through C6)
    uint16_t C[6];
    for (int i = 0; i < 6; i++) {
        uint8_t cmd = 0xA2 + (i * 2);
        sensorSPI->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
        digitalWrite(BARO_CS_PIN, LOW);
        sensorSPI->transfer(cmd);
        uint8_t msb = sensorSPI->transfer(0x00);
        uint8_t lsb = sensorSPI->transfer(0x00);
        digitalWrite(BARO_CS_PIN, HIGH);
        sensorSPI->endTransaction();
        C[i] = (msb << 8) | lsb;
    }

    // Helper to read ADC result after conversion
    auto readADC = [&]() {
        uint32_t result = 0;
        sensorSPI->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
        digitalWrite(BARO_CS_PIN, LOW);
        sensorSPI->transfer(0x00);
        result = ((uint32_t)sensorSPI->transfer(0x00) << 16)
               | ((uint32_t)sensorSPI->transfer(0x00) << 8)
               | ((uint32_t)sensorSPI->transfer(0x00));
        digitalWrite(BARO_CS_PIN, HIGH);
        sensorSPI->endTransaction();
        return result;
    };

    // Perform D1 (pressure) conversion
    sensorSPI->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
    digitalWrite(BARO_CS_PIN, LOW);
    sensorSPI->transfer(0x48);  // CONVERT_D1, OSR=4096
    digitalWrite(BARO_CS_PIN, HIGH);
    sensorSPI->endTransaction();
    delay(10);
    uint32_t D1 = readADC();

    // Perform D2 (temperature) conversion
    sensorSPI->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
    digitalWrite(BARO_CS_PIN, LOW);
    sensorSPI->transfer(0x58);  // CONVERT_D2, OSR=4096
    digitalWrite(BARO_CS_PIN, HIGH);
    sensorSPI->endTransaction();
    delay(10);
    uint32_t D2 = readADC();

    // Report raw readings
    Serial.print(F("Raw Pressure (D1): ")); Serial.println(D1);
    Serial.print(F("Raw Temp     (D2): ")); Serial.println(D2);
    Serial.println(F("=== Test Complete ==="));
}

void runObdFlashTest(int argc, char **argv) {
    Serial.println(F("=== W25Q128JVSIQ Flash Test ==="));

    // Read JEDEC ID
    sensorSPI->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
    digitalWrite(OBD_DR_CS_PIN, LOW);
    sensorSPI->transfer(0x9F);
    uint8_t mfr = sensorSPI->transfer(0x00);
    uint8_t type = sensorSPI->transfer(0x00);
    uint8_t cap = sensorSPI->transfer(0x00);
    digitalWrite(OBD_DR_CS_PIN, HIGH);
    sensorSPI->endTransaction();

    Serial.print(F("JEDEC ID - Mfr: 0x")); Serial.print(mfr, HEX);
    Serial.print(F(", Type: 0x"));            Serial.print(type, HEX);
    Serial.print(F(", Capacity: 0x"));        Serial.println(cap, HEX);

    // Read Status Register (0x05)
    sensorSPI->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
    digitalWrite(OBD_DR_CS_PIN, LOW);
    sensorSPI->transfer(0x05);
    uint8_t status = sensorSPI->transfer(0x00);
    digitalWrite(OBD_DR_CS_PIN, HIGH);
    sensorSPI->endTransaction();

    Serial.print(F("Status Register: 0x")); Serial.println(status, HEX);
    Serial.println(F("=== Flash Test Complete ==="));
}

}
