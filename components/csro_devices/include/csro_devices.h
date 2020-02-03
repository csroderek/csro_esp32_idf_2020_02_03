#ifndef _CSRO_DEVICES_H_
#define _CSRO_DEVICES_H_

#include "csro_common.h"

//csro_devices.c
void csro_device_init(void);
void csro_device_on_connect(esp_mqtt_event_handle_t event);
void csro_device_on_message(esp_mqtt_event_handle_t event);

//csro_motor_csro_3t2r.c
void csro_motor_csro_3t2r_init(void);
void csro_motor_csro_3t2r_on_connect(esp_mqtt_event_handle_t event);
void csro_motor_csro_3t2r_on_message(esp_mqtt_event_handle_t event);

//csro_dlight_csro_3t3scr.c
void csro_dlight_csro_3t3scr_init(void);
void csro_dlight_csro_3t3scr_on_connect(esp_mqtt_event_handle_t event);
void csro_dlight_csro_3t3scr_on_message(esp_mqtt_event_handle_t event);

#endif