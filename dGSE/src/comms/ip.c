#include "comms/card.h"
#include "esp_err.h"
#include "esp_eth_driver.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOSConfig_arch.h"
#include "freertos/idf_additions.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/prot/ethernet.h"
#include "lwip/prot/ip4.h"
#include "portmacro.h"

static QueueHandle_t inputQueue = 0;

esp_err_t IRAM_ATTR ip_frame_handler(esp_eth_handle_t hdl, uint8_t *buf,
                                     uint32_t length, void *priv) {
  if (inputQueue == 0) {
    ESP_LOGI("Ethernet", "Packet received before packet processor start");
    return ESP_ERR_INVALID_STATE;
  }
  struct pbuf *p = pbuf_alloc(PBUF_LINK, (uint16_t)length, PBUF_POOL);
  xQueueSendToBackFromISR(inputQueue, &p, (TickType_t)0);

  return ESP_OK;
}

/* Queued packet handling is necessary because lwips threaded netif input
 * function is not ISR safe If the code is profiled and there are performance
 * issues and they result from quick malloc/free then we need to modify lwip
 * slightly.
 * Modifying LWIP is by far the more optimal solution but it seems somewhat
 * complicated to pull off from a build system perspective the actual change is
 * very simple
 * */

typedef struct {
  struct pbuf *p;
  struct netif ***iftable;
} arp_args;

void processARP(void *pvParameters) {
  /* ew ew ew ew ew ew ew ew ew */
  arp_args *args = pvParameters;
  for (int i = 0; i < CARD_TYPES; i++) {
    for (int j = 0; j < CARD_MAX_ID; j++) {
      struct netif *nif = args->iftable[i][j];
      if (nif == NULL) {
        continue;
      }
      struct pbuf *local_pbuf = pbuf_alloc(PBUF_LINK, args->p->len, PBUF_RAM);
      pbuf_copy(local_pbuf, args->p);
      nif->input(local_pbuf, nif);
    }
  }
  struct netif *carrier = args->iftable[CARD_TYPES][0];
  struct pbuf *final_pbuf = pbuf_alloc(PBUF_LINK, args->p->len, PBUF_RAM);
  pbuf_copy(final_pbuf, args->p);
  carrier->input(final_pbuf, carrier);
  pbuf_free(args->p);
  /* Kill this task */
  vTaskDelete(NULL);
}

void inputPacketProcessingTask(void *pvParameters) {
  /* "Large" number as a contingency so things don't get dropped */
  inputQueue = xQueueCreate(150, sizeof(struct pbuf *));
  struct netif ***iftable = pvParameters;
  ip4_addr_t carrier_ip = CARRIER_IP;

  while (true) {
    struct pbuf *p;
    xQueueReceive(inputQueue, &p, (TickType_t)10);
    struct ip_hdr *hdr =
        (struct ip_hdr *)((uint8_t *)p->payload + SIZEOF_ETH_HDR);

    /* Be careful to make carrier ip not overlap with arp data */
    if (hdr->dest.addr == carrier_ip.addr) {
      struct netif *inf = iftable[CARD_TYPES][0];
      if (inf != NULL)
        inf->input(p, inf);
      continue;
    }
    if ((hdr->_v_hl & 0b11110000) == 0b00100000) {
      uint8_t *address = (uint8_t *)(&hdr->dest.addr);
      /* Remove This. This exists to test whether i am doing endianess wrong */
      if (address[0] == 10 && address[1] == 0) {
        ESP_LOGI("Ethernet", "FIX ME!!!!!!!!!");
        return;
      }
      // use first two bytes as least significant since esp32 is little endian i
      // think
      struct netif *inf = iftable[address[1]][address[0]];
      if (inf == NULL) {
        continue;
      }
      inf->input(p, inf);
      /* No need to free p bc it is consumed by inf->input */
    } else {
      /* Just assuming other shit is ARP packets
       * Soooo many mallocs are needed for This
       * it hurts me :(
       * if there are performance problems we will need to fork lwip
       * srsly there are prob sooo many redundant arp tables being allocated
       * I'm spawning another task to avoid having handling this stuff clog up
       * comms
       * arp packets can be handled lazily so its fine probably
       * Crashes may result from this code if the stack depth is too small which
       * it very well might be
       * */
      arp_args args = {.p = p, .iftable = iftable};
      TaskHandle_t arpHandle;
      xTaskCreate(processARP, "ARP Packet Handler",
                  configMINIMAL_STACK_SIZE + 300, &args, tskIDLE_PRIORITY,
                  &arpHandle);
    }
  }
}
