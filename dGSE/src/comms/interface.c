#include "arch/sys_arch.h"
#include "comms/card.h"
#include "comms/eth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "lwip/err.h"
#include "lwip/memp.h"
#include "lwip/sys.h"
#define ETHARP_TCPIP_ETHINPUT

#include "esp_err.h"
#include "lwip/etharp.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/priv/tcpip_priv.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "netif/ethernet.h"
#include "portmacro.h"
#include <stdint.h>

const ip4_addr_t netmask = {.addr = 0x0A000000};
const ip4_addr_t gateway = {.addr = 0x0A000001};

struct udp_pcb_attach_args {
  channel_t *channel;
  struct netif *interface;
  ip4_addr_t addr;
  udp_recv_fn handler;
  SemaphoreHandle_t sem;
};

static void attach_channel_udp_pcb(void *ctx) {
  struct udp_pcb_attach_args *args = ctx;
  args->channel->pcb = udp_new();
  if (args->channel->pcb != NULL) {
    args->channel->addr = args->addr;
    udp_bind_netif(args->channel->pcb, args->interface);
    udp_recv(args->channel->pcb, args->handler, args->channel);
  }
  xSemaphoreGive(args->sem);
}

/* preinitialize channel object with mac address. will probably change. also
 * udp_recv_fn should use p->payload to access raw packet data w/o header.
 * remember to free packets in handler */
esp_err_t attach_card(channel_t *channel, uint8_t type, uint8_t id,
                      udp_recv_fn handler, struct netif ***iftable,
                      netif_init_fn *init_table) {
  // TODO: ADD NULL CHECKS FOR THIS FN
  portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

  struct netif *interface = malloc(sizeof(struct netif));
  ip4_addr_t addr = {.addr = 0};
  uint8_t addrbuf[4] = {10, 0, type, id};
  /* This probably will cause issues bc i am confused about endianness lmao :p
   */
  addr.addr |=
      (addrbuf[0] << 24) | (addrbuf[1] << 16) | (addrbuf[2] << 8) | addrbuf[3];

  portENTER_CRITICAL_ISR(&spinlock);

  /* Small chance of Race Condition in this Code.
   * I'm not doing it in TCPIP thread bc i think that might be bad and nobody
   * says to???
   */
  netif_add(interface, &addr, &netmask, &gateway, channel, init_table[type],
            tcpip_input);
  portEXIT_CRITICAL_ISR(&spinlock);
  /* This block being preemptible by interrupts might cause issues?? */
  interface->output = etharp_output;
  interface->linkoutput = &link_output;
  interface->mtu = 1500;
  interface->hwaddr_len = 6;
  /* ough............... */
  for (int i = 0; i < NETIF_MAX_HWADDR_LEN; i++) {
    interface->hwaddr[i] = mac_addr[i];
  }
  interface->name[0] = type;
  interface->name[1] = id;
  interface->flags |= NETIF_FLAG_ETHARP;

  /* Make sure interface is recognized */
  etharp_gratuitous(interface);

  iftable[type][id] = interface;

  /* Create UDP Interface for Card on LwIP TCPIP Thread */
  StaticSemaphore_t sem_buf;
  SemaphoreHandle_t sem = xSemaphoreCreateBinaryStatic(&sem_buf);
  xSemaphoreTake(sem, 0);
  struct udp_pcb_attach_args args = {.channel = channel,
                                     .interface = interface,
                                     .addr = addr,
                                     .handler = handler,
                                     .sem = sem};
  tcpip_send_msg_wait_sem(attach_channel_udp_pcb, &args, &sem_buf);

  return ESP_OK;
}

esp_err_t detach_card(uint8_t type, uint8_t id, struct netif ***iftable) {
  portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL_ISR(&spinlock);
  channel_t *channel = iftable[type][id]->state;
  udp_remove(channel->pcb);
  netif_remove(iftable[type][id]);
  /* Will set the component of the iftable past the available card types to the
   * carrier. This is to avoid issues involving null dereferences or sending
   * packets to objs which don't exist */
  iftable[type][id] = iftable[CARD_TYPES][0];
  free(channel);
  portEXIT_CRITICAL_ISR(&spinlock);

  return ESP_OK;
}
esp_err_t attach_carrier(udp_recv_fn handler, struct netif ***iftable,
                         netif_init_fn init) {
  // TODO: ADD NULL CHECKS FOR THIS FN
  portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

  struct netif *interface = malloc(sizeof(struct netif));
  /* This probably will cause issues bc i am confused about endianness lmao :p
   */
  portENTER_CRITICAL_ISR(&spinlock);

  /* Small chance of Race Condition in this Code.
   * I'm not doing it in TCPIP thread bc i think that might be bad and nobody
   * says to???
   */
  ip4_addr_t addr = CARRIER_IP;
  netif_add(interface, &addr, &netmask, &gateway, NULL, init, tcpip_input);
  portEXIT_CRITICAL_ISR(&spinlock);
  /* This block being preemptible by interrupts might cause issues?? */
  interface->output = etharp_output;
  interface->linkoutput = &link_output;
  interface->mtu = 1500;
  interface->hwaddr_len = 6;
  /* ough............... */
  for (int i = 0; i < NETIF_MAX_HWADDR_LEN; i++) {
    interface->hwaddr[i] = mac_addr[i];
  }
  interface->name[0] = 'C';
  interface->name[1] = 'R';
  interface->flags |= NETIF_FLAG_ETHARP;

  /* Make sure interface is recognized */
  etharp_gratuitous(interface);

  iftable[CARD_TYPES][0] = interface;

  /* Create UDP Interface for Card on LwIP TCPIP Thread */
  StaticSemaphore_t sem_buf;
  SemaphoreHandle_t sem = xSemaphoreCreateBinaryStatic(&sem_buf);
  xSemaphoreTake(sem, 0);
  struct udp_pcb_attach_args args = {.channel = NULL,
                                     .interface = interface,
                                     .addr = addr,
                                     .handler = handler,
                                     .sem = sem};
  tcpip_send_msg_wait_sem(attach_channel_udp_pcb, &args, &sem_buf);

  return ESP_OK;
}
