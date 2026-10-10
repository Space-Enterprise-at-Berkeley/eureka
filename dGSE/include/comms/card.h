#include "esp_eth_driver.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/udp.h"

#ifndef _DGSE_CARD_H
#define _DGSE_CARD_H

#define CARD_UDP_PORT 1111

#define CARD_TYPES 4
#define CARD_MAX_ID 10

#define CARRIER_IP {.addr = 0x00000000} // TODO: Make this a part of pio env

typedef struct {
  esp_eth_handle_t *eth_hdl;
  struct udp_pcb *pcb;
  ip4_addr_t addr;
  uint8_t mac_addr[NETIF_MAX_HWADDR_LEN];
  int num;
  void *adc;
} channel_t;
#endif
