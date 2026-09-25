#include "Power.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "proto/Packet_24VSupplyStats.h"

static const char *TAG = "power";

//reads power stats from INA233 and sends to ground station
namespace Power
{
    INA233 ina(INA233_ADDRESS_41);
    float rShunt = 0.004;
    float iMax = 5.0;

    Comms::Packet p;

    void init()
    {
        ina.init(rShunt,iMax);
    }

    void vTaskReadSendPower(void *pvParameters)
    {
        (void)pvParameters;
        while (1)
        {
            // read the ina
            float busVoltage = ina.readBusVoltage();
            float shuntCurrent = ina.readCurrent();
            //float shuntVoltage = ina.readShuntVoltage(); don't need this
            float power = ina.readPower();
            //float avgPower = ina.readAvgPower(); eh maybe?

            //make Packet
            Packet24VSupplyStats::Builder()
                .withSupply24Voltage(busVoltage)
                .withSupply24Current(shuntCurrent)
                .withSupply24Power(power)
                .build()
                .writeRawPacket(&p);

            // emit the packet
            Comms::emitPacketOverAllInterfaces(&p);

            vTaskDelay(pdMS_TO_TICKS(1000)); // once per second
        }
    }

    void print()
    {
        // read the ina
        float busVoltage = ina.readBusVoltage();
        float shuntCurrent = ina.readCurrent();
        //float shuntVoltage = ina.readShuntVoltage(); don't need this
        float power = ina.readPower();
        //float avgPower = ina.readAvgPower(); eh maybe?

        // print the ina
        ESP_LOGI(TAG,
                 "Bus Voltage: %.3f V, Shunt Current: %.3f A, Power: %.3f W",
                 busVoltage, shuntCurrent, power);
    }
}