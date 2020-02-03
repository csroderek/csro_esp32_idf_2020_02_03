idf_component_register(SRCS "csro_common.c"
                    INCLUDE_DIRS "include"
                    REQUIRES spi_flash nvs_flash mqtt)