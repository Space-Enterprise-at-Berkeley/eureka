#include "Barometer.h"

namespace Barometer {
    uint8_t cs = 37;
    uint32_t UPDATE_PERIOD = 50*1000; // 20 Hz
    MS5607 P_Sens = MS5607(cs);

    float altitudeOffset = 0;
    float baroAltitude, baroPressure, baroTemperature;
    float samples = 0;
    float sumPressure = 0;
    int pressureSamples = 0;
    float finalAvgPressure = 0;
    float avgPressure = 0;
    bool calibration = false;
    bool calibrationcompleted = false;
    
    Comms::Packet p;
    
    void init(void) 
    {
       // Serial.begin(19200);
       pinMode(cs, OUTPUT);
       digitalWrite(cs, HIGH);
       P_Sens.begin();
       Comms::registerCallback(PACKET_ID_FCEnableBaroCalibration, en_baro_callback);
       Serial.println("MS5607 initialized");
    }

    uint32_t sampleBaro(void)
    {
        if (!P_Sens.updateConversionCycle()) {
            return P_Sens.CONV_DELAY*1000;
        }
        baroPressure = P_Sens.getPressure();
        baroTemperature = P_Sens.getTemperature();
        baroAltitude = P_Sens.getAltitude();
        // Serial.println();
        // Serial.print("Altitude :  ");
        // Serial.print(baroAltitude);
        // Serial.println(" meters");
        // Serial.print("Pressure :  ");
        // Serial.print(baroPressure);
        // Serial.println(" mBar");
        // Serial.print("Temperature :  ");
        // Serial.print(baroTemperature);
        // Serial.println(" C");

        PacketBaroValues::Builder()
            .withAltitude(baroAltitude)
            .withPressure(baroPressure)
            .withTemperature(baroTemperature)
            .build()
            .writeRawPacket(&p);
        Comms::emitPacketOverAllInterfaces(&p);
        return UPDATE_PERIOD - P_Sens.CONV_DELAY*2*1000;
    }
    uint32_t lastAvgTime = 0;
    uint32_t averageBaro(void){
        if (!P_Sens.updateConversionCycle()) {
            return P_Sens.CONV_DELAY*1000;
        }
        float currentPressure = P_Sens.getPressure();
        
        if(calibration && !calibrationcompleted){
            sumPressure += currentPressure;
            pressureSamples++;
            avgPressure = sumPressure/pressureSamples; 
        }
        else if (calibrationcompleted){
                avgPressure = finalAvgPressure;
        }
        if (micros() - lastAvgTime >= 1000 * 1000) {
            lastAvgTime = micros();
            PacketAvgBaroValues::Builder()
            .withAvgpressure(avgPressure)
            .build()
            .writeRawPacket(&p);
            Comms::emitPacketOverAllInterfaces(&p);
        }
        return UPDATE_PERIOD - P_Sens.CONV_DELAY*2*1000;
    }
    void en_baro_callback(Comms::Packet packet, uint8_t ip) {
        PacketFCEnableBaroCalibration parsed_packet = PacketFCEnableBaroCalibration::fromRawPacket(&packet);
        bool enable = parsed_packet.m_Action;
        if (enable && !calibrationcompleted) {
            calibration = true;
            calibrationcompleted = false;
            sumPressure = 0;
            pressureSamples = 0;
            finalAvgPressure = 0;
        }
        else if (!enable && !calibrationcompleted) {
            calibration = false;
            calibrationcompleted = true;
            if (pressureSamples > 0 && sumPressure > 0) {
                finalAvgPressure = sumPressure/pressureSamples;
                P_Sens.setReferencePressure(finalAvgPressure);
            }
        }
    }

    // uint32_t zeroAltitude()
    // {
    //     static bool conversionStarted = false;

    //     if (!conversionStarted) {
    //         P_Sens.startConversionCycle();
    //         conversionStarted = true;

    //         return P_Sens.CONV_TIME_REMAINING;
    //     }

    //     if (!P_Sens.updateConversionCycle()) {
    //         return P_Sens.CONV_TIME_REMAINING;
    //     }

    //     conversionStarted = false;

    //     altitudeOffset = P_Sens.getAltitude();
    //     samples++;
    //     return 1000;
    // }
}