idf_component_register(SRCS "csro_devices.c" "csro_dlight_csro_3t3scr.c" "csro_motor_csro_3t2r.c"
                    INCLUDE_DIRS "include"
                    REQUIRES mqtt csro_common)