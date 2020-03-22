#ifndef _CSRO_COMMON_H
#define _CSRO_COMMON_H

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_smartconfig.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/timer.h"
#include "cJSON.h"
#include "lwip/sockets.h"

//#define USE_CLOUD_SERVER
//#define USE_ASSIGNED_ROUTER

#define MANUFACTURER "南京金星宇节能技术有限公司"
#define SOFT_VERSION "2020-02-03"

#define DLIGHT_CSRO_3T3SCR

/*
#define MOTOR_CSRO_3T2R
#define DLIGHT_CSRO_3T3SCR
*/

typedef struct
{
    uint8_t restore_flag;
    uint8_t router_flag;
    char router_ssid[50];
    char router_pass[50];
    uint8_t mac[6];
    char mac_str[20];
    uint8_t ip[4];
    char ip_str[20];
    char host_name[30];
    char dev_type[20];
    uint32_t power_cnt;
    uint32_t time_run;
} csro_system;

typedef struct
{
    char id[50];
    char name[50];
    char pass[50];
    char sub_topic[100];
    char pub_topic[100];
    char lwt_topic[100];
    char content[1000];
    char broker[50];
    char uri[60];
    char prefix[50];
} csro_mqtt;

extern csro_system sysinfo;
extern csro_mqtt mqttinfo;
extern esp_mqtt_client_handle_t mqttclient;

//csro_common.c
void csro_main(void);
void csro_reset_router(void);

//csro_smart.c
void csro_smart_task(void);

//csro_mqtt.c
void csro_mqtt_task(void);

#endif