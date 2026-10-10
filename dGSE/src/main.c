
#include "FreeRTOSConfig.h"
#include "comms/card.h"
#include "comms/eth.h"
#include "comms/interface.h"
#include "comms/ip.h"
#include "esp_eth_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOSConfig_arch.h"
#include "freertos/idf_additions.h"
#include "lwip/err.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "portmacro.h"

static struct netif *iftable[CARD_TYPES][CARD_MAX_ID];

void carrier_input(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                   const ip_addr_t *addr, uint16_t port) {
  ESP_LOGI("CARRIER", "Received Packet on Carrier from %s containing bytes: %s",
           ip4addr_ntoa(&addr->u_addr.ip4), p->payload);
  pbuf_free(p);
}

err_t carrier_init(struct netif *carrier_if) {
  ESP_LOGI("CARRIER", "Carrier Interface Initialized");
  return ERR_OK;
}

void app_main() {
  esp_eth_handle_t *handle;
  TaskHandle_t packet_task;
  setup_eth_link(&handle, ip_frame_handler, iftable);
  xTaskCreate(inputPacketProcessingTask, "Input Packet Processing Task",
              configMINIMAL_STACK_SIZE + 300, iftable, configMAX_PRIORITIES - 1,
              &packet_task);
  xSemaphoreTake(eth_startup_sem, 10);
  attach_carrier(carrier_input, (struct netif ***)iftable, carrier_init);
}
