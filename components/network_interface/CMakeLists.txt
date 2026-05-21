set(srcs "network_interface.c" "eth_interface.c")
set(requires driver esp_wifi esp_eth esp_netif esp_timer nvs_flash improv_wifi morsemicro__halow status_led)

if(CONFIG_SNAPCLIENT_USE_HALOW)
    list(APPEND srcs "halow_interface.c")
else()
    list(APPEND srcs "wifi_interface.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES ${requires})

# wifi_provisioning is only referenced when CONFIG_ENABLE_HALOW_PROVISIONING is
# set; idf_component_optional_requires is honoured AFTER idf_component_register
# (unlike conditional PRIV_REQUIRES, which is evaluated in a phase where
# CONFIG_* is not yet visible).
if(CONFIG_ENABLE_HALOW_PROVISIONING)
    idf_component_optional_requires(PRIVATE wifi_provisioning)
endif()
