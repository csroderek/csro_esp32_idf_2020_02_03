#include "csro_common.h"
#include "csro_devices.h"

esp_netif_t *wifi_netif;

static void time_stamp_task(void *args)
{
    while (true)
    {
        printf("Run %d mins. Free heap is %d\r\n", sysinfo.time_run, esp_get_free_heap_size());
        vTaskDelay(60000 / portTICK_RATE_MS);
        sysinfo.time_run++;
    }
    vTaskDelete(NULL);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    if (event->event_id == MQTT_EVENT_CONNECTED)
    {
        csro_device_on_connect(event);
    }
    else if (event->event_id == MQTT_EVENT_DATA)
    {
        csro_device_on_message(event);
    }
}

static void udp_receive_mqtt_server(void)
{
#ifdef USE_CLOUD_SERV
    static char udp_buffer[256] = "{\"server\":\"csro.net.cn\"}";
#else
    static char udp_buffer[256];
    int udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udp_sock < 0)
    {
        return;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5000);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(udp_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        close(udp_sock);
        return;
    }
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);
    bzero(udp_buffer, 256);
    int len = recvfrom(udp_sock, udp_buffer, sizeof(udp_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
    close(udp_sock);
    if (len < 0)
    {
        return;
    }
#endif
    cJSON *json = cJSON_Parse(udp_buffer);
    cJSON *server;
    if (json != NULL)
    {
        server = cJSON_GetObjectItem(json, "server");
        if ((server != NULL) && (server->type == cJSON_String) && strlen(server->valuestring) >= 7)
        {
            if (strcmp((char *)server->valuestring, (char *)mqttinfo.broker) != 0)
            {
                strcpy((char *)mqttinfo.broker, (char *)server->valuestring);
                sprintf(mqttinfo.uri, "mqtt://%s", mqttinfo.broker);
                printf("%s\r\n", mqttinfo.uri);
                if (mqttclient != NULL)
                {
                    esp_mqtt_client_destroy(mqttclient);
                }
                esp_mqtt_client_config_t mqtt_cfg = {
                    .client_id = mqttinfo.id,
                    .username = mqttinfo.name,
                    .password = mqttinfo.pass,
                    .uri = mqttinfo.uri,
                    .keepalive = 60,
                    .lwt_topic = mqttinfo.lwt_topic,
                    .lwt_msg = "offline",
                    .lwt_retain = 1,
                    .lwt_qos = 1,
                };
                mqttclient = esp_mqtt_client_init(&mqtt_cfg);
                esp_mqtt_client_register_event(mqttclient, ESP_EVENT_ANY_ID, mqtt_event_handler, mqttclient);
                esp_mqtt_client_start(mqttclient);
            }
        }
    }
    cJSON_Delete(json);
}

static void udp_server_task(void *args)
{
    while (true)
    {
        udp_receive_mqtt_server();
        vTaskDelay(60000 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    printf("WIFI_EVENT: %d\r\n", event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_netif_set_hostname(wifi_netif, sysinfo.host_name);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        esp_wifi_connect();
    }
}

void csro_mqtt_task(void)
{
    printf("Strating mqtt task......\r\n");
    xTaskCreate(time_stamp_task, "time_stamp_task", 2048, NULL, configMAX_PRIORITIES - 10, NULL);

    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_WIFI_STA();
    wifi_netif = esp_netif_new(&netif_config);

    assert(wifi_netif);
    esp_netif_attach_wifi_station(wifi_netif);
    esp_wifi_set_default_wifi_sta_handlers();

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);

    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.sta.ssid, (char *)sysinfo.router_ssid);
    strcpy((char *)wifi_config.sta.password, (char *)sysinfo.router_pass);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
    xTaskCreate(udp_server_task, "udp_server_task", 2048, NULL, configMAX_PRIORITIES - 7, NULL);
}