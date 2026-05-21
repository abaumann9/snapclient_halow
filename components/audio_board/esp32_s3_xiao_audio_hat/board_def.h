/*
 * ESP32-S3 XIAO + Audio HAT board definition
 *
 * DAC: PCM5102A
 * Button: GPIO17 (WS2812B-style, no HW debounce) - reserved for DPP pairing
 * LED: GPIO16 (WS2812B-2020) - status LED
 */

#ifndef _AUDIO_BOARD_DEFINITION_H_
#define _AUDIO_BOARD_DEFINITION_H_

#include "driver/gpio.h"

/* PCM5102A DAC - no I2C control needed */
#define FUNC_AUDIO_CODEC_EN (1)
#define BOARD_PA_GAIN (0)
#define HEADPHONE_DETECT (-1)

/* PCM5102A mute pin (active low) */
#define PCM5102A_MUTE_GPIO GPIO_NUM_40
#ifndef CONFIG_PCM5102A_MUTE_PIN
#define CONFIG_PCM5102A_MUTE_PIN PCM5102A_MUTE_GPIO
#endif

/* I2S pins */
#define BOARD_I2S_MCLK_PIN GPIO_NUM_48
#define BOARD_I2S_BCK_PIN GPIO_NUM_47
#define BOARD_I2S_LRCK_PIN GPIO_NUM_39
#define BOARD_I2S_DATAOUT_PIN GPIO_NUM_38

/* No PA enable on this board */
#define PA_ENABLE_GPIO (-1)

/* No SD card */
#define SDCARD_OPEN_FILE_NUM_MAX 5
#define SDCARD_INTR_GPIO (-1)
#define SDCARD_PWR_CTRL (-1)

/* Button - reserved for future DPP pairing (not yet implemented) */
#define BOARD_BUTTON_GPIO GPIO_NUM_11

/* WS2812B status LED */
#define BOARD_LED_GPIO GPIO_NUM_10

/* No I2C needed for PCM5102A */
#define BOARD_I2C_SDA (-1)
#define BOARD_I2C_SCL (-1)

/* Button IDs (unused for now, kept for API compatibility) */
#define BUTTON_VOLUP_ID 0
#define BUTTON_VOLDOWN_ID 1
#define BUTTON_MUTE_ID 2
#define BUTTON_SET_ID 3

#define INPUT_KEY_NUM 0

extern audio_hal_func_t AUDIO_CODEC_PCM5102A_DEFAULT_HANDLE;
#define AUDIO_CODEC_DEFAULT_CONFIG()               \
  {                                                \
      .adc_input = AUDIO_HAL_ADC_INPUT_LINE1,      \
      .dac_output = AUDIO_HAL_DAC_OUTPUT_ALL,      \
      .codec_mode = AUDIO_HAL_CODEC_MODE_DECODE,   \
      .i2s_iface =                                 \
          {                                        \
              .mode = AUDIO_HAL_MODE_SLAVE,        \
              .fmt = AUDIO_HAL_I2S_NORMAL,         \
              .samples = AUDIO_HAL_48K_SAMPLES,    \
              .bits = AUDIO_HAL_BIT_LENGTH_16BITS, \
          },                                       \
  };

#endif
