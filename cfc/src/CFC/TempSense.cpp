#include "TempSense.h"
#include "proto/Packet_FCTemperature.h"

//reads temp stats from LM75
namespace TempSense
{
    LM75 lm(0x4F);
    float sendRate = 500 * 1000; // 0.5 second
    
    Comms::Packet p;

    void init()
    {
        // let'sa gooo!
        // Serial.begin(921600); Comms takes care of this

        //Wire.setClock(400000); 
        //Wire.setPins(1,2);
        //Wire.begin();
    }

    uint32_t task_readSendTemp()
    {
        //very short wow
       float tC = lm.temp();

        // print();
        //make Packet
            PacketFCTemperature::Builder()
            .withTemp(tC)
            .build()
            .writeRawPacket(&p);

        // emit the packet
        Comms::emitPacketOverAllInterfaces(&p);

        return sendRate; // .5 second
    }

    void print()
    {
        float tC = lm.temp();
        Serial.print("Temp: ");
        Serial.print(tC);
        Serial.println(" °C");
    }
}