#include "csro_devices.h"

#ifdef MOTOR_CSRO_3T2R

#define THRESHOLD 10

#define TOUCH_01_NUM 0
#define TOUCH_02_NUM 2
#define TOUCH_03_NUM 3

#define RELAY_OPEN_NUM GPIO_NUM_27
#define RELAY_CLOSE_NUM GPIO_NUM_14

#define LED_01_NUM GPIO_NUM_19
#define LED_02_NUM GPIO_NUM_18
#define LED_03_NUM GPIO_NUM_5
#define GPIO_OUTPUT_PIN_SEL ((1ULL << RELAY_OPEN_NUM) | (1ULL << RELAY_CLOSE_NUM) | (1ULL << LED_01_NUM) | (1ULL << LED_02_NUM) | (1ULL << LED_03_NUM))

typedef enum
{
    STOP = 0,
    OPEN = 1,
    CLOSE = 2,
    STOP_TO_OPEN = 3,
    STOP_TO_CLOSE = 4,
} motor_state;

typedef enum
{
    NORMAL = 0,
    FLASH = 1,
} led_mode;

motor_state motor = STOP;
led_mode led = NORMAL;
uint8_t led_on[3] = {0, 0, 0};

static void motor_csro_3t2r_mqtt_update(void)
{
    if (mqttclient != NULL)
    {
        printf("mq update!\r\n");
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

static void motor_csro_3t2r_relay_led_task(void *args)
{
    static motor_state last_state = STOP;
    static uint16_t relay_count_100ms = 0;
    static uint16_t led_count_100ms = 0;
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    while (true)
    {
        bool update = false;
        if (last_state != motor)
        {
            printf("state %d\r\n", motor);
            last_state = motor;
            update = true;
            if (motor == STOP || motor == STOP_TO_CLOSE || motor == STOP_TO_OPEN)
            {
                gpio_set_level(RELAY_OPEN_NUM, 0);
                gpio_set_level(RELAY_CLOSE_NUM, 0);
                relay_count_100ms = 5;
            }
            else if (motor == OPEN)
            {
                gpio_set_level(RELAY_OPEN_NUM, 1);
                gpio_set_level(RELAY_CLOSE_NUM, 0);
                relay_count_100ms = 600;
            }
            else if (motor == CLOSE)
            {
                gpio_set_level(RELAY_OPEN_NUM, 0);
                gpio_set_level(RELAY_CLOSE_NUM, 1);
                relay_count_100ms = 600;
            }
        }

        if (relay_count_100ms > 0)
        {
            relay_count_100ms--;
            if (relay_count_100ms == 0)
            {
                motor = (motor == STOP_TO_CLOSE) ? CLOSE : (motor == STOP_TO_OPEN) ? OPEN : STOP;
            }
        }
        led_count_100ms = (led_count_100ms + 1) % 10;
        if (led == FLASH && led_count_100ms >= 5)
        {
            gpio_set_level(LED_01_NUM, led_on[0]);
            gpio_set_level(LED_02_NUM, led_on[1]);
            gpio_set_level(LED_03_NUM, led_on[2]);
        }
        else
        {
            gpio_set_level(LED_01_NUM, led_on[0]);
            gpio_set_level(LED_02_NUM, led_on[1]);
            gpio_set_level(LED_03_NUM, led_on[2]);
        }
        if (update)
        {
            motor_csro_3t2r_mqtt_update();
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void motor_csro_3t2r_touch_task(void *args)
{
    static uint16_t first_touch_value[4];
    static uint16_t single_holdtime[3];
    static uint16_t triple_holdtime;
    touch_pad_init();
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    for (int i = 0; i < 4; i++)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        touch_pad_config(i, 0);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        touch_pad_read(i, &first_touch_value[i]);
    }

    while (true)
    {
        uint8_t touch_state[3];
        led_mode led_temp = NORMAL;
        for (uint8_t i = 0; i < 4; i++)
        {
            uint16_t touch_value;
            touch_pad_read(i, &touch_value);
            if (i != 1)
            {
                touch_state[i == 0 ? 0 : i == 2 ? 1 : 2] = (first_touch_value[i] > touch_value && (first_touch_value[i] - touch_value >= THRESHOLD)) ? 1 : 0;
                led_on[i == 0 ? 0 : i == 2 ? 1 : 2] = touch_state[i == 0 ? 0 : i == 2 ? 1 : 2];
            }
        }
        if (touch_state[0] + touch_state[1] + touch_state[2] == 1)
        {
            if (triple_holdtime >= 1000 && triple_holdtime <= 1500)
            {
                esp_restart();
            }
            triple_holdtime = 0;
            single_holdtime[0] = (single_holdtime[0] + 1) * touch_state[0];
            single_holdtime[1] = (single_holdtime[1] + 1) * touch_state[1];
            single_holdtime[2] = (single_holdtime[2] + 1) * touch_state[2];
            if (single_holdtime[0] == 4)
            {
                motor = (motor == STOP || motor == STOP_TO_CLOSE) ? OPEN : (motor == CLOSE) ? STOP_TO_OPEN : motor;
                // if (motor == STOP || motor == STOP_TO_CLOSE)
                // {
                //     motor = OPEN;
                // }
                // else if (motor == CLOSE)
                // {
                //     motor = STOP_TO_OPEN;
                // }
            }
            else if (single_holdtime[1] == 4)
            {
                motor = STOP;
            }
            else if (single_holdtime[2] == 4)
            {
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
        else if (touch_state[0] + touch_state[1] + touch_state[2] == 3)
        {
            triple_holdtime++;
            printf("triple_holdtime %d\r\n", triple_holdtime);
            single_holdtime[0] = 0;
            single_holdtime[1] = 0;
            single_holdtime[2] = 0;
            if (triple_holdtime >= 1000 && triple_holdtime <= 1500)
            {
                led_temp = FLASH;
            }
            else if (triple_holdtime > 1500)
            {
                esp_restart();
            }
        }
        else
        {
            if (triple_holdtime >= 1000 && triple_holdtime <= 1500)
            {
                esp_restart();
            }
            triple_holdtime = 0;
            single_holdtime[0] = 0;
            single_holdtime[1] = 0;
            single_holdtime[2] = 0;
        }
        led = led_temp;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void csro_motor_csro_3t2r_init(void)
{
    printf("csro_motor_csro_3t2r_init\r\n");
    xTaskCreate(motor_csro_3t2r_relay_led_task, "motor_csro_3t2r_relay_led_task", 2048, NULL, configMAX_PRIORITIES - 8, NULL);
    xTaskCreate(motor_csro_3t2r_touch_task, "motor_csro_3t2r_touch_task", 2048, NULL, configMAX_PRIORITIES - 6, NULL);
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
    cJSON_AddStringToObject(config_json, "pl_open", "up");
    cJSON_AddStringToObject(config_json, "pl_stop", "stop");
    cJSON_AddStringToObject(config_json, "pl_cls", "down");
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
        if (strncmp("up", event->data, event->data_len) == 0)
        {
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
            motor = STOP;
        }
        else if (strncmp("down", event->data, event->data_len) == 0)
        {
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