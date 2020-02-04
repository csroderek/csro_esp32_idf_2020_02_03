idf_component_register(SRCS "csro_common.c" "csro_mqtt.c" "csro_smart.c"
                    INCLUDE_DIRS "include"
                    REQUIRES spi_flash nvs_flash mqtt csro_devices json esp_netif)