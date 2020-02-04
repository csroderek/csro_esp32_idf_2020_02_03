
#include "csro_common.h"

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        printf("WIFI_EVENT_STA_START\r\n");
        esp_smartconfig_set_type(SC_TYPE_AIRKISS);
        smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
        esp_smartconfig_start(&cfg);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        printf("WIFI_EVENT_STA_DISCONNECTED\r\n");
        esp_smartconfig_stop();
        esp_smartconfig_set_type(SC_TYPE_AIRKISS);
        smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
        esp_smartconfig_start(&cfg);
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD)
    {
        printf("SC_EVENT_GOT_SSID_PSWD\r\n");
        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        strcpy(sysinfo.router_ssid, (const char *)wifi_config.sta.ssid);
        strcpy(sysinfo.router_pass, (const char *)wifi_config.sta.password);
        printf("ssid:%s.\r\npass:%s.\r\n", sysinfo.router_ssid, sysinfo.router_pass);
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true)
        {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }
        esp_wifi_disconnect();
        esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
        esp_wifi_connect();
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE)
    {
        printf("SC_EVENT_SEND_ACK_DONE\r\n");
        esp_smartconfig_stop();
        nvs_handle handle;
        nvs_open("system", NVS_READWRITE, &handle);
        nvs_set_str(handle, "router_ssid", sysinfo.router_ssid);
        nvs_set_str(handle, "router_pass", sysinfo.router_pass);
        nvs_set_u8(handle, "router_flag", 1);
        nvs_commit(handle);
        nvs_close(handle);
        esp_restart();
    }
}

void csro_smart_task(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL);
    esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
}