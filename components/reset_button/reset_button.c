#include "reset_button.h"

#if CONFIG_RESET_BUTTON_ENABLED

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "status_led.h"

static const char *TAG = "RST_BTN";

#define POLL_PERIOD_MS 50

static int pressed_level(void) {
#if CONFIG_RESET_BUTTON_ACTIVE_LOW
  return 0;
#else
  return 1;
#endif
}

static void erase_credentials(void) {
  ESP_LOGW(TAG, "Erasing persisted Wi-Fi / HaLow credentials");
  nvs_handle_t h;
  /* esp_wifi stores its config under the "nvs.net80211" namespace. */
  if (nvs_open("nvs.net80211", NVS_READWRITE, &h) == ESP_OK) {
    nvs_erase_all(h);
    nvs_commit(h);
    nvs_close(h);
  }
  /* HaLow provisioning namespace (see halow_interface.c). */
  if (nvs_open("halow_prov", NVS_READWRITE, &h) == ESP_OK) {
    nvs_erase_all(h);
    nvs_commit(h);
    nvs_close(h);
  }
}

static void reset_button_task(void *arg) {
  const gpio_num_t gpio = (gpio_num_t)CONFIG_RESET_BUTTON_GPIO;
  const int active = pressed_level();
  const TickType_t hold_ticks = pdMS_TO_TICKS(CONFIG_RESET_BUTTON_HOLD_MS);

  TickType_t press_start = 0;
  bool was_pressed = false;
  bool fired = false;

  while (1) {
    int level = gpio_get_level(gpio);
    bool is_pressed = (level == active);

    if (is_pressed && !was_pressed) {
      press_start = xTaskGetTickCount();
      fired = false;
    } else if (!is_pressed && was_pressed) {
      fired = false;
    } else if (is_pressed && !fired) {
      if ((xTaskGetTickCount() - press_start) >= hold_ticks) {
        fired = true;
        ESP_LOGW(TAG, "Long-press detected, clearing credentials");
        status_led_set_state(STATUS_LED_FACTORY_RESET);
        erase_credentials();
        vTaskDelay(pdMS_TO_TICKS(1500));
        esp_restart();
      }
    }

    was_pressed = is_pressed;
    vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS));
  }
}

esp_err_t reset_button_init(void) {
  if (CONFIG_RESET_BUTTON_GPIO < 0) {
    ESP_LOGW(TAG, "reset button disabled (GPIO < 0)");
    return ESP_OK;
  }

  gpio_config_t cfg = {
      .pin_bit_mask = BIT64(CONFIG_RESET_BUTTON_GPIO),
      .mode = GPIO_MODE_INPUT,
#if CONFIG_RESET_BUTTON_ACTIVE_LOW
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
#else
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_ENABLE,
#endif
      .intr_type = GPIO_INTR_DISABLE,
  };
  esp_err_t err = gpio_config(&cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
    return err;
  }

  if (xTaskCreate(reset_button_task, "rst_btn", 3072, NULL,
                  tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
    ESP_LOGE(TAG, "failed to create reset_button task");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "reset button on GPIO%d (hold %d ms)",
           CONFIG_RESET_BUTTON_GPIO, CONFIG_RESET_BUTTON_HOLD_MS);
  return ESP_OK;
}

#endif /* CONFIG_RESET_BUTTON_ENABLED */
