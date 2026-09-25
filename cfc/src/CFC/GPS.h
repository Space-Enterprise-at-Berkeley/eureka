#include <Arduino.h>
namespace GPS {
    void init();
    uint32_t task_readGPS();
    uint32_t task_readGPSExtra();
    uint32_t task_readGPSSatInfo();
}