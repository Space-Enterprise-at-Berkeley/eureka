#include "comms/card.h"
#include "comms/eth.h"
#include "esp_err.h"
#include "esp_eth_driver.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOSConfig_arch.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "lwip/ip4_addr.h"
#include "lwip/memp.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/prot/ethernet.h"
#include "lwip/prot/ip4.h"
#include "portmacro.h"
#include "uthash.h"

static QueueHandle_t inputQueue = 0;

extern char mac_addr[6];

extern SemaphoreHandle_t eth_startup_sem;
struct pckt {
  uint32_t len;
  uint8_t *buf;
  struct pbuf_custom p;
};

LWIP_MEMPOOL_DECLARE(RX_POOL, 100, sizeof(struct pckt),
                     "Mempool for storing RX packet descriptors");
void free_packet(struct pbuf *p) {
  struct pckt *pack = (struct pckt *)((uint8_t *)p - offsetof(struct pckt, p));
  free(pack->buf);
  LWIP_MEMPOOL_FREE(RX_POOL, pack);
}
esp_err_t IRAM_ATTR ip_frame_handler(esp_eth_handle_t hdl, uint8_t *buf,
                                     uint32_t length, void *priv) {
  if (inputQueue == 0) {
    ESP_LOGI("Ethernet", "Packet received before packet processor start");
    return ESP_ERR_INVALID_STATE;
  }
  struct pckt *packet = (struct pckt *)LWIP_MEMPOOL_ALLOC(RX_POOL);
  packet->buf = buf;
  /* hacky way of getting rid of fcs */
  packet->len = length - 4;
  xQueueSendToBackFromISR(inputQueue, &packet, (TickType_t)0);

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
  card_t **cardtable;
} arp_args;

struct __attribute__((packed)) arp_hdr {
  unsigned short arp_hd;
  unsigned char arp_pr_first;
  unsigned char arp_pr_second;
  unsigned char arp_hdl;
  unsigned char arp_prl;
  unsigned short arp_op;
  unsigned char arp_sha[6];
  unsigned char arp_spa[4];
  unsigned char arp_dha[6];
  unsigned char arp_dpa[4];
};

void inputPacketProcessingTask(void *pvParameters) {
  /* "Large" number as a contingency so things don't get dropped */
  inputQueue = xQueueCreate(100, sizeof(struct pckt *));
  card_t **cardtable = pvParameters;
  ip4_addr_t carrier_ip = CARRIER_IP;

  while (true) {
    struct pckt *packet;
    if (xQueueReceive(inputQueue, &packet, (TickType_t)10) == pdPASS) {
      struct ip_hdr *hdr =
          (struct ip_hdr *)((uint8_t *)packet->buf + SIZEOF_ETH_HDR);
      packet->p.custom_free_function = free_packet;

      struct pbuf *p =
          pbuf_alloced_custom(PBUF_RAW, packet->len, PBUF_REF, &packet->p,
                              packet->buf, packet->len);
      /* Be careful to make carrier ip not overlap with arp data */
      if (hdr->dest.addr == carrier_ip.addr) {
        card_t *inf;
        ESP_LOGI("IP", "CARRIER RECVD PACKET");
        HASH_FIND(hh, *cardtable, &carrier_ip, sizeof(ip4_addr_t), inf);
        if (inf != NULL)
          inf->interface->input(p, inf->interface);

        continue;
      }
      if ((hdr->_v_hl & 0b11110000) == 0b00100000 ||
          (hdr->_v_hl & 0b00001111) == 0b00000010) {
        ESP_LOGI("IP", "IP PACKET RECVD");
        card_t *inf;
        ip4_addr_t dest = {.addr = hdr->dest.addr};
        HASH_FIND(hh, *cardtable, &dest, sizeof(ip4_addr_t), inf);
        if (inf == NULL) {
          ESP_LOGI("ETH", "CARD AT %s NOT FOUND!!!", ip4addr_ntoa(&dest));
          pbuf_free(p);
          continue;
        }
        inf->interface->input(p, inf->interface);
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
         * Crashes may result from this code if the stack depth is too small
         * which it very well might be
         * */
        /* Do this with an arp header instead. might work */
        card_t *inf = NULL;
        struct arp_hdr *arp_header =
            (struct arp_hdr *)((uint8_t *)p->payload + SIZEOF_ETH_HDR);
        ip4_addr_t dest = {.addr = *(uint32_t *)arp_header->arp_dpa};
        if (arp_header->arp_pr_first != 0x08) {
          pbuf_free(p);
          continue;
        }
        HASH_FIND(hh, *cardtable, &dest, sizeof(ip4_addr_t), inf);
        ESP_LOGI("ETH", "RECEIVED ARP FOR %s !!!", ip4addr_ntoa(&dest));
        if (inf != NULL) {
          inf->interface->input(p, inf->interface);
          ESP_LOGI("ETH", "ARP HIT!!");
        }
      }
    }
  }
}
