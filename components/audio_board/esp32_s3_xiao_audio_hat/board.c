/*
 * ESP32-S3 XIAO + Audio HAT board initialization
 */

#include "board.h"

#include "audio_mem.h"
#include "esp_log.h"

extern audio_hal_func_t AUDIO_CODEC_PCM5102A_DEFAULT_HANDLE;

static const char *TAG = "XIAO_S3_AUDIO_HAT";

static audio_board_handle_t board_handle = 0;

audio_board_handle_t audio_board_init(void) {
  if (board_handle) {
    ESP_LOGW(TAG, "The board has already been initialized!");
    return board_handle;
  }
  board_handle =
      (audio_board_handle_t)audio_calloc(1, sizeof(struct audio_board_handle));
  AUDIO_MEM_CHECK(TAG, board_handle, return NULL);
  board_handle->audio_hal = audio_board_codec_init();
  return board_handle;
}

audio_hal_handle_t audio_board_codec_init(void) {
  audio_hal_codec_config_t audio_codec_cfg = AUDIO_CODEC_DEFAULT_CONFIG();
  audio_hal_handle_t codec_hal =
      audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_PCM5102A_DEFAULT_HANDLE);
  AUDIO_NULL_CHECK(TAG, codec_hal, return NULL);
  return codec_hal;
}

audio_hal_handle_t audio_board_adc_init(void) { return NULL; }

audio_board_handle_t audio_board_get_handle(void) { return board_handle; }

esp_err_t audio_board_deinit(audio_board_handle_t audio_board) {
  esp_err_t ret = ESP_OK;
  ret |= audio_hal_deinit(audio_board->audio_hal);
  audio_free(audio_board);
  board_handle = NULL;
  return ret;
}
