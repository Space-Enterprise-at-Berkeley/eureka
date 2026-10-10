#include "esp_attr.h"
#include "esp_err.h"
#include "esp_eth_driver.h"

#ifndef _DGSE_IP_H
#define _DGSE_IP_H
esp_err_t IRAM_ATTR ip_frame_handler(esp_eth_handle_t hdl, uint8_t *buf,
                                     uint32_t length, void *priv);
void inputPacketProcessingTask(void *pvParameters);

#endif
