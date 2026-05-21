/*
    HaLow (802.11ah) related functionality
    Mirrors wifi_interface.c but uses the morsemicro/halow registry component.

    When CONFIG_ENABLE_HALOW_PROVISIONING is set, the ESP wifi_provisioning
    manager is run over a 2.4 GHz SoftAP on first boot. The captured SSID +
    passphrase are persisted to NVS (namespace "halow_prov") and re-used on
    subsequent boots. Holding the reset button erases that namespace.
*/
#include "halow_interface.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_types.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "network_interface.h"
#include "status_led.h"

#include "mmhalow.h"
#include "mmwlan.h"

#if ENABLE_HALOW_PROVISIONING
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include "nvs.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"
#endif

static const char *TAG = "HALOW_IF";

#define HALOW_NVS_NAMESPACE "halow_prov"
#define HALOW_NVS_KEY_SSID  "ssid"
#define HALOW_NVS_KEY_PSK   "psk"

static esp_netif_ip_info_t ip_info = {{0}, {0}, {0}};
static bool connected = false;
static SemaphoreHandle_t connIpSemaphoreHandle = NULL;

static char s_ssid[33];
static char s_psk[65];

static void sta_status_callback(enum mmwlan_sta_state sta_state) {
  switch (sta_state) {
    case MMWLAN_STA_DISABLED:
      ESP_LOGI(TAG, "HaLow STA disabled");
      break;
    case MMWLAN_STA_CONNECTING:
      ESP_LOGI(TAG, "HaLow STA connecting");
      break;
    case MMWLAN_STA_CONNECTED:
      ESP_LOGI(TAG, "HaLow STA connected");
      break;
  }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data) {
  ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
  if (!network_is_our_netif(NETWORK_INTERFACE_DESC_STA, event->esp_netif)) {
    return;
  }

  xSemaphoreTake(connIpSemaphoreHandle, portMAX_DELAY);
  memcpy((void *)&ip_info, (const void *)&event->ip_info,
         sizeof(esp_netif_ip_info_t));
  connected = true;
  xSemaphoreGive(connIpSemaphoreHandle);

  ESP_LOGI(TAG, "HaLow Got IP Address");
  ESP_LOGI(TAG, "~~~~~~~~~~~");
  ESP_LOGI(TAG, "HALOWIP:" IPSTR, IP2STR(&ip_info.ip));
  ESP_LOGI(TAG, "HALOWMASK:" IPSTR, IP2STR(&ip_info.netmask));
  ESP_LOGI(TAG, "HALOWGW:" IPSTR, IP2STR(&ip_info.gw));
  ESP_LOGI(TAG, "~~~~~~~~~~~");
}

static void lost_ip_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data) {
  ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
  if (!network_is_our_netif(NETWORK_INTERFACE_DESC_STA, event->esp_netif)) {
    return;
  }

  xSemaphoreTake(connIpSemaphoreHandle, portMAX_DELAY);
  memcpy((void *)&ip_info, (const void *)&event->ip_info,
         sizeof(esp_netif_ip_info_t));
  connected = false;
  xSemaphoreGive(connIpSemaphoreHandle);

  ESP_LOGI(TAG, "HaLow Lost IP Address");
}

bool halow_get_ip(esp_netif_ip_info_t *ip) {
  xSemaphoreTake(connIpSemaphoreHandle, portMAX_DELAY);

  if (ip) {
    memcpy((void *)ip, (const void *)&ip_info, sizeof(esp_netif_ip_info_t));
  }
  bool _connected = connected;

  xSemaphoreGive(connIpSemaphoreHandle);

  return _connected;
}

#if ENABLE_HALOW_PROVISIONING

static SemaphoreHandle_t s_creds_sem;
static SemaphoreHandle_t s_prov_end_sem;

static esp_err_t load_creds_from_nvs(void) {
  nvs_handle_t h;
  if (nvs_open(HALOW_NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
    return ESP_ERR_NVS_NOT_FOUND;
  }

  size_t len = sizeof(s_ssid);
  esp_err_t err = nvs_get_str(h, HALOW_NVS_KEY_SSID, s_ssid, &len);
  if (err != ESP_OK) {
    nvs_close(h);
    return err;
  }
  len = sizeof(s_psk);
  err = nvs_get_str(h, HALOW_NVS_KEY_PSK, s_psk, &len);
  /* psk may legitimately be empty -> ESP_ERR_NVS_NOT_FOUND is also OK */
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    s_psk[0] = '\0';
    err = ESP_OK;
  }
  nvs_close(h);
  return err;
}

static esp_err_t save_creds_to_nvs(void) {
  nvs_handle_t h;
  esp_err_t err = nvs_open(HALOW_NVS_NAMESPACE, NVS_READWRITE, &h);
  if (err != ESP_OK) {
    return err;
  }
  err = nvs_set_str(h, HALOW_NVS_KEY_SSID, s_ssid);
  if (err == ESP_OK) {
    err = nvs_set_str(h, HALOW_NVS_KEY_PSK, s_psk);
  }
  if (err == ESP_OK) {
    err = nvs_commit(h);
  }
  nvs_close(h);
  return err;
}

static void prov_event_handler(void *arg, esp_event_base_t base, int32_t id,
                               void *data) {
  if (base == WIFI_PROV_EVENT) {
    switch (id) {
      case WIFI_PROV_START:
        ESP_LOGI(TAG, "Provisioning started");
        break;
      case WIFI_PROV_CRED_RECV: {
        wifi_sta_config_t *cfg = (wifi_sta_config_t *)data;
        strlcpy(s_ssid, (const char *)cfg->ssid, sizeof(s_ssid));
        strlcpy(s_psk, (const char *)cfg->password, sizeof(s_psk));
        ESP_LOGI(TAG, "Creds received. SSID=\"%s\"", s_ssid);
        xSemaphoreGive(s_creds_sem);
        break;
      }
      case WIFI_PROV_CRED_FAIL:
        ESP_LOGW(TAG,
                 "Manager 2.4GHz connect test failed (expected; HaLow takes "
                 "over)");
        break;
      case WIFI_PROV_CRED_SUCCESS:
        ESP_LOGI(TAG, "Manager reported CRED_SUCCESS");
        break;
      case WIFI_PROV_END:
        ESP_LOGI(TAG, "Provisioning ended");
        xSemaphoreGive(s_prov_end_sem);
        break;
      default:
        break;
    }
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
    ESP_LOGI(TAG, "Phone associated to provisioning SoftAP");
  }
}

static esp_err_t run_softap_provisioning(void) {
  s_creds_sem = xSemaphoreCreateBinary();
  s_prov_end_sem = xSemaphoreCreateBinary();
  if (!s_creds_sem || !s_prov_end_sem) {
    return ESP_ERR_NO_MEM;
  }

  esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

  wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(
      WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));

  wifi_prov_mgr_config_t prov_cfg = {
      .scheme = wifi_prov_scheme_softap,
      .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
  };
  ESP_ERROR_CHECK(wifi_prov_mgr_init(prov_cfg));

  uint8_t mac[6];
  ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
  char service_name[16];
  snprintf(service_name, sizeof(service_name), "PROV_%02X%02X%02X", mac[3],
           mac[4], mac[5]);

  const char *pop = CONFIG_HALOW_PROV_POP;

  ESP_LOGI(TAG, "Starting SoftAP provisioning");
  ESP_LOGI(TAG, "  SoftAP SSID : %s", service_name);
  ESP_LOGI(TAG, "  PoP         : %s", pop);
  ESP_LOGI(TAG, "  Phone app   : ESP SoftAP Provisioning");

  status_led_set_state(STATUS_LED_PROVISIONING);

  ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, pop,
                                                   service_name, NULL));

  xSemaphoreTake(s_creds_sem, portMAX_DELAY);
  /* Brief pause so the phone gets the protocomm response before tear down. */
  vTaskDelay(pdMS_TO_TICKS(2000));

  wifi_prov_mgr_stop_provisioning();
  xSemaphoreTake(s_prov_end_sem, portMAX_DELAY);
  wifi_prov_mgr_deinit();

  ESP_ERROR_CHECK(esp_event_handler_unregister(
      WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler));
  ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &prov_event_handler));

  esp_wifi_stop();
  esp_wifi_deinit();
  esp_netif_destroy_default_wifi(ap_netif);
  esp_netif_destroy_default_wifi(sta_netif);

  vSemaphoreDelete(s_creds_sem);
  vSemaphoreDelete(s_prov_end_sem);
  s_creds_sem = NULL;
  s_prov_end_sem = NULL;

  return save_creds_to_nvs();
}

#endif /* ENABLE_HALOW_PROVISIONING */

void halow_start(void) {
  if (!connIpSemaphoreHandle) {
    connIpSemaphoreHandle = xSemaphoreCreateMutex();
  }

  /* Register IP event handlers */
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &got_ip_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP,
                                             &lost_ip_event_handler, NULL));

#if ENABLE_HALOW_PROVISIONING
  if (load_creds_from_nvs() != ESP_OK) {
    ESP_LOGI(TAG, "No stored HaLow credentials, starting provisioning");
    esp_err_t err = run_softap_provisioning();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Provisioning failed: %s", esp_err_to_name(err));
      return;
    }
  } else {
    ESP_LOGI(TAG, "Loaded HaLow credentials from NVS (SSID=\"%s\")", s_ssid);
  }
#else
  strlcpy(s_ssid, HALOW_SSID, sizeof(s_ssid));
  strlcpy(s_psk, HALOW_PASSWORD, sizeof(s_psk));
#endif

  /* Initialize the HaLow stack (mmhal, mmwlan, netif, firmware boot) */
  ESP_ERROR_CHECK(mmhalow_init(NULL));
  mmhalow_print_version_info();

  /* Configure SSID and passphrase */
  mmhalow_wifi_config_t halow_config = {
      .sta = MMWLAN_STA_ARGS_INIT,
  };
  size_t ssid_len = strlen(s_ssid);
  size_t psk_len = strlen(s_psk);
  memcpy(halow_config.sta.ssid, s_ssid, ssid_len);
  halow_config.sta.ssid_len = ssid_len;
  memcpy(halow_config.sta.passphrase, s_psk, psk_len);
  halow_config.sta.passphrase_len = psk_len;
  halow_config.sta.security_type = psk_len > 0 ? MMWLAN_SAE : MMWLAN_OPEN;
  ESP_ERROR_CHECK(mmhalow_set_config(WIFI_IF_STA, &halow_config));

  /* Start STA connection (non-blocking) */
  ESP_ERROR_CHECK(mmhalow_connect(sta_status_callback));

  ESP_LOGI(TAG, "HaLow init done, connecting to %s", s_ssid);
}
