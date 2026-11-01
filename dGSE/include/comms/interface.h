#include "comms/card.h"
#include "comms/eth.h"
#include "esp_err.h"
#include "esp_eth_driver.h"
#include "lwip/netif.h"
#include "lwip/udp.h"

#ifndef _DGSE_INTERFACE_H
#define _DGSE_INTERFACE_H
esp_err_t attach_card(channel_t *channel, uint8_t type, uint8_t id,
                      udp_recv_fn handler, card_t **cardtable,
                      netif_init_fn *init_table);

esp_err_t detach_card(uint8_t type, uint8_t id, card_t **cardtable);
esp_err_t attach_carrier(udp_recv_fn handler, card_t **cardtable,
                         netif_init_fn init, esp_eth_handle_t *hdl);
#endif
