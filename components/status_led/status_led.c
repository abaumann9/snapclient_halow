#include "status_led.h"

#if CONFIG_STATUS_LED_ENABLED

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "led_strip.h"

static const char *TAG = "STATUS_LED";
static led_strip_handle_t led_strip = NULL;
static TimerHandle_t blink_timer = NULL;
static bool led_on = false;
static status_led_state_t active_state = STATUS_LED_OFF;

typedef struct {
  uint8_t r, g, b;
  uint16_t on_ms;
  uint16_t off_ms; // 0 = solid (no blink)
} led_pattern_t;

#define B 40

static const led_pattern_t patterns[] = {
    [STATUS_LED_OFF]               = {  0,   0,   0,    0,    0},
    [STATUS_LED_NETWORK_WAITING]   = {  0,   0,   B,  300,  700}, // blink blue
    [STATUS_LED_SERVER_CONNECTING] = {  0,   0,   B,    0,    0}, // solid blue
    [STATUS_LED_CONNECTED]         = {  0,   B,   0, 1000,    0}, // green 1s then off
    [STATUS_LED_RESYNCING]         = {  B, B/2,   0,  200,  200}, // blink orange fast
    [STATUS_LED_ERROR]             = {  B,   0,   0,    0,    0}, // solid red
    [STATUS_LED_PROVISIONING]      = {  B,   0,   B,  500,  500}, // purple blink
    [STATUS_LED_FACTORY_RESET]     = {  B,   0,   0,  100,  100}, // red fast blink
};

static void set_pixel(uint8_t r, uint8_t g, uint8_t b) {
  if (!led_strip) return;
  led_strip_set_pixel(led_strip, 0, r, g, b);
  led_strip_refresh(led_strip);
}

static void blink_timer_cb(TimerHandle_t timer) {
  const led_pattern_t *p = &patterns[active_state];

  if (active_state == STATUS_LED_CONNECTED) {
    // One-shot: turn off after green flash
    xTimerStop(timer, 0);
    set_pixel(0, 0, 0);
    active_state = STATUS_LED_OFF;
    return;
  }

  // Guard: if state was changed to solid/off before callback ran, just stop
  if (p->off_ms == 0 || p->on_ms == 0) {
    xTimerStop(timer, 0);
    return;
  }

  // Toggle blink
  led_on = !led_on;
  if (led_on) {
    set_pixel(p->r, p->g, p->b);
    xTimerChangePeriod(timer, pdMS_TO_TICKS(p->on_ms), 0);
  } else {
    set_pixel(0, 0, 0);
    xTimerChangePeriod(timer, pdMS_TO_TICKS(p->off_ms), 0);
  }
}

esp_err_t status_led_init(int gpio_num) {
  if (gpio_num < 0) return ESP_OK;

  led_strip_config_t strip_config = {
      .strip_gpio_num = gpio_num,
      .max_leds = 1,
      .led_pixel_format = LED_PIXEL_FORMAT_GRB,
      .led_model = LED_MODEL_WS2812,
      .flags.invert_out = false,
  };
  led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 10 * 1000 * 1000,
      .flags.with_dma = false,
  };

  esp_err_t ret =
      led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "LED strip init failed: %d", ret);
    return ret;
  }

  led_strip_clear(led_strip);

  blink_timer =
      xTimerCreate("led_blink", pdMS_TO_TICKS(100), pdTRUE, NULL, blink_timer_cb);

  ESP_LOGI(TAG, "LED initialized on GPIO%d", gpio_num);
  return ESP_OK;
}

void status_led_set_state(status_led_state_t state) {
  if (!led_strip) return;
  if (state == active_state) return;

  active_state = state;
  const led_pattern_t *p = &patterns[state];

  xTimerStop(blink_timer, 0);

  if (state == STATUS_LED_OFF) {
    set_pixel(0, 0, 0);
    return;
  }

  // Set initial color immediately
  set_pixel(p->r, p->g, p->b);
  led_on = true;

  if (state == STATUS_LED_CONNECTED) {
    // One-shot: timer fires once after on_ms to turn off
    xTimerChangePeriod(blink_timer, pdMS_TO_TICKS(p->on_ms), 0);
  } else if (p->off_ms > 0) {
    // Blinking: first timer fires after on_ms to start toggle cycle
    xTimerChangePeriod(blink_timer, pdMS_TO_TICKS(p->on_ms), 0);
  }
  // Solid states: no timer, pixel is already set
}

void status_led_set_rgb(uint8_t r, uint8_t g, uint8_t b) {
  set_pixel(r, g, b);
}

#endif // CONFIG_STATUS_LED_ENABLED
