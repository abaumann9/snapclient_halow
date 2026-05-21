#ifndef _STATUS_LED_H_
#define _STATUS_LED_H_

#include "esp_err.h"
#include "sdkconfig.h"

typedef enum {
  STATUS_LED_OFF,
  STATUS_LED_NETWORK_WAITING,   // blinking blue
  STATUS_LED_SERVER_CONNECTING, // solid blue
  STATUS_LED_CONNECTED,         // green flash then off
  STATUS_LED_RESYNCING,         // blinking orange
  STATUS_LED_ERROR,             // solid red
  STATUS_LED_PROVISIONING,      // blinking purple (waiting for creds)
  STATUS_LED_FACTORY_RESET,     // fast blinking red (creds clear in progress)
} status_led_state_t;

#if CONFIG_STATUS_LED_ENABLED
esp_err_t status_led_init(int gpio_num);
void status_led_set_state(status_led_state_t state);
void status_led_set_rgb(uint8_t r, uint8_t g, uint8_t b);
#else
static inline esp_err_t status_led_init(int gpio_num) { return ESP_OK; }
static inline void status_led_set_state(status_led_state_t state) { (void)state; }
static inline void status_led_set_rgb(uint8_t r, uint8_t g, uint8_t b) { (void)r; (void)g; (void)b; }
#endif

#endif
