#include "comms/card.h"
#include "esp_err.h"
#include "lwip/netif.h"
#include "lwip/udp.h"

#ifndef _DGSE_INTERFACE_H
#define _DGSE_INTERFACE_H
esp_err_t attach_card(channel_t *channel, uint8_t type, uint8_t id,
                      udp_recv_fn handler, struct netif ***iftable,
                      netif_init_fn *init_table);

esp_err_t detach_card(channel_t *channel, uint8_t type, uint8_t id,
                      struct netif ***iftable);
esp_err_t attach_carrier(udp_recv_fn handler, struct netif ***iftable,
                         netif_init_fn init);
#endif
