idf_component_register(SRCS "reset_button.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES driver nvs_flash esp_system status_led)
