/*
 * Common Flight Computer Serial Diagnostic
 * DO NOT FLY
 *
 */


#include <Arduino.h>
#include "Common.h"
#include <Cmd.h>
#include <SPI.h>
#include <ADS8167.h>
#include "diagnostics.h"

#pragma Message("REFLASH BEFORE FLIGHT - Don't fly this code!")

#include <INA233.h>
#include <LSM6DSOSensor.h>
//#include <USBComms.h>

#define ADDRESS_TEMPSENSOR 0x4F
#define INA_SHUNT_RESISTANCE 0.004

#define MID_ACCEL_CS_PIN 40
#define BARO_CS_PIN 5
#define GPS_CS_PIN 3
#define HIGH_ACCEL_CS_PIN 39
#define ADC_CS_PIN 37
#define EXT_DR_CS_PIN 10
#define OBD_DR_CS_PIN 36

#define SENSORSPI_MOSI 15
#define SENSORSPI_MISO 16
#define SENSORSPI_SCLK 17
static const int spiClk = 1000000;  // 1 MHz

// --- H3LIS331DL register map & constants ---
#define H3_WHO_AM_I_REG     0x0F
#define H3_WHO_AM_I_VAL     0x32
#define H3_CTRL_REG1        0x20
#define H3_OUT_X_L          0x28
#define H3_OUT_Y_L          0x2A
#define H3_OUT_Z_L          0x2C

HardwareSerial tlmRadioSerial(1);
HardwareSerial lvRadioSerial(2);

#define TLM_RADIO_RX_PIN 19
#define TLM_RADIO_TX_PIN 18
#define LV_RADIO_RX_PIN 33
#define LV_RADIO_TX_PIN 26

#define LV_RADIO_BAUD 115200
#define TLM_RADIO_BAUD 115200

SPIClass sensorSPI(FSPI);

INA233 ina(INA233_ADDRESS_41, Wire);
LSM6DSOSensor midAccGyr(&sensorSPI, MID_ACCEL_CS_PIN, spiClk);
ADS8167 ptADC;
SPIClass *spi2 = new SPIClass(HSPI);

uint32_t task_commandListener();
void registerDiagnosticCommands();

Task taskTable[] = {
    {task_commandListener, 0, true}
};

#define TASK_COUNT (sizeof(taskTable) / sizeof (struct Task))


// Main setup
void setup() {
    // Serial.begin(115200);
    
    /*
    pinMode(SENSORSPI_MISO, INPUT);
    pinMode(SENSORSPI_SCLK, OUTPUT);
    pinMode(SENSORSPI_MOSI, OUTPUT);
    */
    
    pinMode(MID_ACCEL_CS_PIN, OUTPUT);
    pinMode(BARO_CS_PIN, OUTPUT);
    pinMode(GPS_CS_PIN, OUTPUT);
    pinMode(HIGH_ACCEL_CS_PIN, OUTPUT);
    pinMode(ADC_CS_PIN, OUTPUT);
    pinMode(EXT_DR_CS_PIN, OUTPUT);
    pinMode(OBD_DR_CS_PIN, OUTPUT);
    
    // pull all these idiots high so that they don't all scream at one another
    digitalWrite(MID_ACCEL_CS_PIN, HIGH);
    digitalWrite(BARO_CS_PIN, HIGH);
    digitalWrite(GPS_CS_PIN, HIGH);
    digitalWrite(HIGH_ACCEL_CS_PIN, HIGH);
    digitalWrite(ADC_CS_PIN, HIGH);
    digitalWrite(EXT_DR_CS_PIN, HIGH);
    digitalWrite(OBD_DR_CS_PIN, HIGH);
    delay(500); // let it simmer

	/*
    for (int i = 0; i < 5; i++) {
        digitalWrite(SENSORSPI_SCLK, HIGH);
        digitalWrite(SENSORSPI_MOSI, HIGH);
        //digitalWrite(SENSORSPI_MISO, HIGH);
        delay(1000);
        digitalWrite(SENSORSPI_SCLK, LOW);
        digitalWrite(SENSORSPI_MOSI, LOW);
        //digitalWrite(SENSORSPI_MISO, LOW);
        delay(1000);
    }
	*/
	tlmRadioSerial.begin(TLM_RADIO_BAUD, SERIAL_8N1, TLM_RADIO_RX_PIN, TLM_RADIO_TX_PIN);
	lvRadioSerial.begin(LV_RADIO_BAUD, SERIAL_8N1, LV_RADIO_RX_PIN, LV_RADIO_TX_PIN);
	FCDiagnostics::setSerial(&tlmRadioSerial, &lvRadioSerial);

    // I2C bus on IO1 & 2
    Wire.begin(1, 2);

    // Configure Temperature Sensor
    Wire.beginTransmission(ADDRESS_TEMPSENSOR);
    Wire.write(0x01); // config register
    Wire.write(0x00); // Continuous operation, normal operation
    Wire.endTransmission();

    // INA233 Power Monitor
    ina.init(INA_SHUNT_RESISTANCE,5.0);

    // init SPI for mid-range accelerometer
    sensorSPI.begin(SENSORSPI_SCLK, SENSORSPI_MISO, SENSORSPI_MOSI);
    FCDiagnostics::setSensorSPI(&sensorSPI);
    
    // activate and check if working
    digitalWrite(MID_ACCEL_CS_PIN, LOW);
    delay(10);
    if (!midAccGyr.begin()) {
        Serial.println("ERROR: LSM6DSO not detected, check wiring");
    }
    digitalWrite(MID_ACCEL_CS_PIN, HIGH);
    delay(10);
    
    // set up PT ADC
    ptADC.init(spi2, 39, 38);
    ptADC.setAllInputsSeparate();
    ptADC.enableOTFMode();

    CmdParse::cmdInit(&Serial);
    registerDiagnosticCommands();

    while(1) {
        // main loop here to avoid arduino overhead
        for(uint32_t i = 0; i < TASK_COUNT; i++) { // for each task, execute if next time >= current time
            uint32_t ticks = micros(); // current time in microseconds
            if (taskTable[i].nexttime - ticks > UINT32_MAX / 2 && taskTable[i].enabled) {
                uint32_t delayoftask = taskTable[i].taskCall();
                if (delayoftask == 0) {
                    taskTable[i].enabled = false;
                }
                else {
                    taskTable[i].nexttime = ticks + delayoftask;
                }
            }
        }
        //USBComms::processWaitingPackets();
    }
}

void loop() {}

/*
 * Task: Command listener
 * Polls the command listener.
 */
uint32_t task_commandListener() {

    CmdParse::cmdPoll();
    return 1000 * 1000;	// very second
}

// Yes, yes I know this is a stupid hack - SD
void powerMonTestRunner(int argc, char **argv) {
    FCDiagnostics::runPowerMonitorTest(ina);
}

void spiSpamTestRunner(int argc, char **argv) {
	FCDiagnostics::spamSPIBus(argc, argv, &sensorSPI);
}

void runHighAccelTest(int argc, char **argv) {
	FCDiagnostics::runHighAccelTest(argc, argv, &sensorSPI);
}

void registerDiagnosticCommands() {
    CmdParse::cmdAdd("scan-i2c", FCDiagnostics::runI2CScan);
    CmdParse::cmdAdd("test-temperature", FCDiagnostics::runTemperatureSensorTest);
    CmdParse::cmdAdd("test-powermon", powerMonTestRunner);
    CmdParse::cmdAdd("test-highaccel",  runHighAccelTest);
    CmdParse::cmdAdd("spam-spi", spiSpamTestRunner); // ridiculous command
    CmdParse::cmdAdd("gpio", FCDiagnostics::setGPIO);
    CmdParse::cmdAdd("tlm-radio-getversion", FCDiagnostics::getTlmRadioVersionInConfig);
    CmdParse::cmdAdd("test-barometer", FCDiagnostics::runBarometerTest);
    CmdParse::cmdAdd("test-obd-bb", FCDiagnostics::runObdFlashTest);
    
}
