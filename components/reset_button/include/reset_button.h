#ifndef _RESET_BUTTON_H_
#define _RESET_BUTTON_H_

#include "esp_err.h"
#include "sdkconfig.h"

#if CONFIG_RESET_BUTTON_ENABLED
/*
 * Initialise a long-press detector on CONFIG_RESET_BUTTON_GPIO.
 * Holding the button for CONFIG_RESET_BUTTON_HOLD_MS will:
 *   1. set the status LED to STATUS_LED_FACTORY_RESET,
 *   2. erase persisted Wi-Fi/HaLow credentials from NVS,
 *   3. reboot the device.
 */
esp_err_t reset_button_init(void);
#else
static inline esp_err_t reset_button_init(void) { return ESP_OK; }
#endif

#endif
