#include "csro_devices.h"

#ifdef MOTOR_CSRO_3T2R

#define TOUCH_01_NUM GPIO_NUM_4
#define TOUCH_02_NUM GPIO_NUM_2
#define TOUCH_03_NUM GPIO_NUM_15

#define RELAY_OPEN_NUM GPIO_NUM_27
#define RELAY_CLOSE_NUM GPIO_NUM_14
#define LED_01_NUM GPIO_NUM_5
#define LED_02_NUM GPIO_NUM_18
#define LED_03_NUM GPIO_NUM_19
#define TOUCH_EN_NUM GPIO_NUM_16

#define GPIO_INPUT_PIN_SEL ((1ULL << TOUCH_01_NUM) | (1ULL << TOUCH_02_NUM) | (1ULL << TOUCH_03_NUM))
#define GPIO_OUTPUT_PIN_SEL ((1ULL << RELAY_OPEN_NUM) | (1ULL << RELAY_CLOSE_NUM) | (1ULL << LED_01_NUM) | (1ULL << LED_02_NUM) | (1ULL << LED_03_NUM) | (1ULL << TOUCH_EN_NUM))

typedef enum
{
    STOP = 0,
    OPEN = 1,
    CLOSE = 2,
    STOP_TO_OPEN = 3,
    STOP_TO_CLOSE = 4,
} motor_state;

motor_state motor = STOP;

static void motor_csro_3t2r_mqtt_update(void)
{
    if (mqttclient != NULL)
    {
        cJSON *state_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(state_json, "state", 50);
        cJSON_AddNumberToObject(state_json, "run", sysinfo.time_run);
        char *out = cJSON_PrintUnformatted(state_json);
        strcpy(mqttinfo.content, out);
        free(out);
        cJSON_Delete(state_json);
        sprintf(mqttinfo.pub_topic, "csro/%s/%s/state", sysinfo.mac_str, sysinfo.dev_type);
        esp_mqtt_client_publish(mqttclient, mqttinfo.pub_topic, mqttinfo.content, 0, 0, 1);
    }
}

static void motor_csro_3t2r_relay_task(void *args)
{
    while (true)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void motor_csro_3t2r_touch_task(void *args)
{
    while (true)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        gpio_set_level(LED_01_NUM, gpio_get_level(TOUCH_01_NUM));
        gpio_set_level(LED_02_NUM, gpio_get_level(TOUCH_02_NUM));
        gpio_set_level(LED_03_NUM, gpio_get_level(TOUCH_03_NUM));
    }
    vTaskDelete(NULL);
}

void csro_motor_csro_3t2r_init(void)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_set_level(TOUCH_EN_NUM, 1);
    xTaskCreate(motor_csro_3t2r_touch_task, "motor_csro_3t2r_touch_task", 2048, NULL, configMAX_PRIORITIES - 6, NULL);
    xTaskCreate(motor_csro_3t2r_relay_task, "motor_csro_3t2r_relay_task", 2048, NULL, configMAX_PRIORITIES - 8, NULL);
}

void csro_motor_csro_3t2r_on_connect(esp_mqtt_event_handle_t event)
{
    printf("csro_motor_csro_3t2r_on_connect\r\n");
    sprintf(mqttinfo.sub_topic, "csro/%s/%s/set/#", sysinfo.mac_str, sysinfo.dev_type);
    esp_mqtt_client_subscribe(event->client, mqttinfo.sub_topic, 0);

    char prefix[50], name[50], deviceid[50];
    sprintf(mqttinfo.pub_topic, "csro/cover/%s_%s/config", sysinfo.mac_str, sysinfo.dev_type);
    sprintf(prefix, "csro/%s/%s", sysinfo.mac_str, sysinfo.dev_type);
    sprintf(name, "%s_%s_0", sysinfo.mac_str, sysinfo.dev_type);
    sprintf(deviceid, "%s_%s", sysinfo.mac_str, sysinfo.dev_type);

    cJSON *config_json = cJSON_CreateObject();
    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(config_json, "~", prefix);
    cJSON_AddStringToObject(config_json, "name", name);
    cJSON_AddStringToObject(config_json, "unique_id", name);
    cJSON_AddStringToObject(config_json, "cmd_t", "~/set");
    cJSON_AddStringToObject(config_json, "pos_t", "~/state");
    cJSON_AddStringToObject(config_json, "avty_t", "~/available");
    cJSON_AddStringToObject(config_json, "pl_open", "open");
    cJSON_AddStringToObject(config_json, "pl_stop", "stop");
    cJSON_AddStringToObject(config_json, "pl_cls", "close");
    cJSON_AddStringToObject(config_json, "val_tpl", "{{value_json.state}}");
    cJSON_AddStringToObject(config_json, "opt", "false");
    cJSON_AddItemToObject(config_json, "dev", device);
    cJSON_AddStringToObject(device, "ids", deviceid);
    cJSON_AddStringToObject(device, "name", deviceid);
    cJSON_AddStringToObject(device, "mf", MANUFACTURER);
    cJSON_AddStringToObject(device, "mdl", "MOTOR_CSRO_3T2R");
    cJSON_AddStringToObject(device, "sw", SOFT_VERSION);

    char *out = cJSON_PrintUnformatted(config_json);
    strcpy(mqttinfo.content, out);
    free(out);
    cJSON_Delete(config_json);
    esp_mqtt_client_publish(event->client, mqttinfo.pub_topic, mqttinfo.content, 0, 0, 1);

    sprintf(mqttinfo.pub_topic, "csro/%s/%s/available", sysinfo.mac_str, sysinfo.dev_type);
    esp_mqtt_client_publish(event->client, mqttinfo.pub_topic, "online", 0, 0, 1);
    motor_csro_3t2r_mqtt_update();
}

void csro_motor_csro_3t2r_on_message(esp_mqtt_event_handle_t event)
{
    char topic[50];
    sprintf(topic, "csro/%s/%s/set", sysinfo.mac_str, sysinfo.dev_type);
    if (strncmp(topic, event->topic, event->topic_len) == 0)
    {
        if (strncmp("open", event->data, event->data_len) == 0)
        {
            // printf("open\r\n");
            if (motor == STOP || motor == STOP_TO_CLOSE)
            {
                motor = OPEN;
            }
            else if (motor == CLOSE)
            {
                motor = STOP_TO_OPEN;
            }
        }
        else if (strncmp("stop", event->data, event->data_len) == 0)
        {
            // printf("stop\r\n");
            motor = STOP;
        }
        else if (strncmp("close", event->data, event->data_len) == 0)
        {
            // printf("close\r\n");
            if (motor == STOP || motor == STOP_TO_OPEN)
            {
                motor = CLOSE;
            }
            else if (motor == OPEN)
            {
                motor = STOP_TO_CLOSE;
            }
        }
    }
}

#endif