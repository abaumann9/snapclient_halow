#ifndef _HALOW_INTERFACE_H_
#define _HALOW_INTERFACE_H_

#include <stdbool.h>

#include "esp_netif.h"

#define ENABLE_HALOW_PROVISIONING CONFIG_ENABLE_HALOW_PROVISIONING

#if !ENABLE_HALOW_PROVISIONING
#define HALOW_SSID CONFIG_HALOW_SSID
#define HALOW_PASSWORD CONFIG_HALOW_PASSWORD
#endif

bool halow_get_ip(esp_netif_ip_info_t *ip);
void halow_start(void);

#endif /* _HALOW_INTERFACE_H_ */
