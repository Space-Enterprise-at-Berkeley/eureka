#ifndef _DGSE_ETH_H
#define _DGSE_ETH_H

#include "esp_err.h"
#include "esp_eth_driver.h"
#include "freertos/idf_additions.h"
#include "interface.h"
#include "lwip/err.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "uthash.h"
#define ETH_MODULE_SPI_SPEED_MHZ                                               \
  26 /* needs to be 26 MHz because we are using GPIO pins */
#define ETH_MODULE_SPI_CS_GPIO 10
#define ETH_MODULE_SPI_INT_GPIO 9
#define ETH_MODULE_SPI_MISO_GPIO 13
#define ETH_MODULE_SPI_MOSI_GPIO 11
#define ETH_MODULE_SPI_CLK_GPIO 12

esp_err_t
setup_eth_link(esp_eth_handle_t **hdl,
               esp_err_t (*frame_handler)(esp_eth_handle_t hdl, uint8_t *buf,
                                          uint32_t length, void *priv),
               card_t **cardtable);

err_t link_output(struct netif *netif, struct pbuf *p);

#endif
