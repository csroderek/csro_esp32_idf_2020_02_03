#include "csro_devices.h"

#ifdef DLIGHT_CSRO_3T3SCR

#define TOUCH_01_NUM GPIO_NUM_18
#define TOUCH_02_NUM GPIO_NUM_17
#define TOUCH_03_NUM GPIO_NUM_16

#define AC_ZERO_NUM GPIO_NUM_34
#define SCR_01_NUM GPIO_NUM_26
#define SCR_02_NUM GPIO_NUM_32
#define SCR_03_NUM GPIO_NUM_33
#define TOUCH_EN_NUM GPIO_NUM_19

#define GPIO_INPUT_PIN_SEL ((1ULL << TOUCH_01_NUM) | (1ULL << TOUCH_02_NUM) | (1ULL << TOUCH_03_NUM))
#define GPIO_OUTPUT_PIN_SEL ((1ULL << SCR_01_NUM) | (1ULL << SCR_02_NUM) | (1ULL << SCR_03_NUM) | (1ULL << TOUCH_EN_NUM))
#define GPIO_INTR_PIN_SEL (1ULL << AC_ZERO_NUM)

#define LEDC_TOTAL_NUM 3
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CH0_GPIO 23
#define LEDC_CH0_CHANNEL LEDC_CHANNEL_0
#define LEDC_CH1_GPIO 22
#define LEDC_CH1_CHANNEL LEDC_CHANNEL_1
#define LEDC_CH2_GPIO 21
#define LEDC_CH2_CHANNEL LEDC_CHANNEL_2

uint16_t bright_index[101] = {
    740, 732, 725, 717, 710, 703, 696, 689, 683, 676, 670,
    664, 658, 653, 647, 641, 636, 631, 625, 620, 615,
    610, 605, 600, 595, 590, 585, 580, 575, 571, 566,
    561, 557, 552, 547, 543, 538, 534, 529, 525, 520,
    515, 511, 507, 502, 497, 493, 488, 484, 479, 475,
    470, 466, 461, 457, 452, 448, 443, 438, 434, 429,
    424, 419, 414, 410, 405, 400, 395, 390, 385, 379,
    374, 369, 363, 358, 352, 347, 341, 335, 329, 323,
    316, 310, 303, 296, 289, 282, 274, 266, 258, 249,
    240, 230, 219, 207, 194, 180, 163, 141, 112, 100};

ledc_channel_config_t ledc_channel[LEDC_TOTAL_NUM] = {
    {.channel = LEDC_CH0_CHANNEL, .duty = 0, .gpio_num = LEDC_CH0_GPIO, .speed_mode = LEDC_MODE, .timer_sel = LEDC_TIMER},
    {.channel = LEDC_CH1_CHANNEL, .duty = 0, .gpio_num = LEDC_CH1_GPIO, .speed_mode = LEDC_MODE, .timer_sel = LEDC_TIMER},
    {.channel = LEDC_CH2_CHANNEL, .duty = 0, .gpio_num = LEDC_CH2_GPIO, .speed_mode = LEDC_MODE, .timer_sel = LEDC_TIMER},
};

typedef struct
{
    uint8_t target;
    uint8_t value;
} dim_light;
dim_light dlight;

uint16_t timer_cnt = 0;
uint16_t gpio_isr_cnt = 0;

static void dlight_csro_3t2scr_mqtt_update(void)
{
    if (mqttclient != NULL)
    {
        cJSON *root_json = cJSON_CreateObject();
        cJSON *state_json = NULL;
        cJSON_AddItemToObject(root_json, "state", state_json = cJSON_CreateObject());
        cJSON_AddNumberToObject(root_json, "run", sysinfo.time_run);
        cJSON_AddNumberToObject(state_json, "bright", dlight.target);
        cJSON_AddNumberToObject(state_json, "on", dlight.target == 0 ? 0 : 1);
        char *out = cJSON_PrintUnformatted(root_json);
        strcpy(mqttinfo.content, out);
        free(out);
        cJSON_Delete(root_json);
        sprintf(mqttinfo.pub_topic, "csro/%s/%s/state", sysinfo.mac_str, sysinfo.dev_type);
        esp_mqtt_client_publish(mqttclient, mqttinfo.pub_topic, mqttinfo.content, 0, 0, 1);
    }
}

void timer_group_isr(void *para)
{
    static DRAM_ATTR bool pulse_timer = false;
    timer_spinlock_take(TIMER_GROUP_0);
    if (timer_group_get_intr_status_in_isr(TIMER_GROUP_0) & TIMER_INTR_T0)
    {
        timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
        if (pulse_timer == false)
        {
            uint64_t timer_counter_value = timer_group_get_counter_value_in_isr(TIMER_GROUP_0, TIMER_0);
            timer_counter_value += (uint64_t)(100);
            gpio_set_level(SCR_01_NUM, dlight.value > 0 ? 0 : 1);
            gpio_set_level(SCR_02_NUM, dlight.value > 0 ? 0 : 1);
            gpio_set_level(SCR_03_NUM, dlight.value > 0 ? 0 : 1);
            timer_group_set_alarm_value_in_isr(TIMER_GROUP_0, TIMER_0, timer_counter_value);
            pulse_timer = true;
        }
        else
        {
            pulse_timer = false;
            gpio_set_level(SCR_01_NUM, 1);
            gpio_set_level(SCR_02_NUM, 1);
            gpio_set_level(SCR_03_NUM, 1);
            timer_pause(TIMER_GROUP_0, TIMER_0);
        }
    }
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_0);
    timer_spinlock_give(TIMER_GROUP_0);
}

static void gpio_isr_handler(void *arg)
{
    gpio_isr_cnt++;
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, bright_index[dlight.value] * 10 - 800);
    timer_start(TIMER_GROUP_0, TIMER_0);
}

static void dlight_csro_3t3scr_touch_task(void *args)
{
    static uint16_t hold_time[3];
    static bool update = false;
    while (true)
    {

        uint8_t touch_statue[3] = {gpio_get_level(TOUCH_01_NUM), gpio_get_level(TOUCH_02_NUM), gpio_get_level(TOUCH_03_NUM)};
        // printf("touch:%d,%d,%d   count:%d,%d\r\n", touch_statue[0], touch_statue[1], touch_statue[2], gpio_isr_cnt, timer_cnt);
        for (uint8_t i = 0; i < LEDC_TOTAL_NUM; i++)
        {
            ledc_set_duty(ledc_channel[i].speed_mode, ledc_channel[i].channel, touch_statue[i] == 0 ? 0 : 7500);
            ledc_update_duty(ledc_channel[i].speed_mode, ledc_channel[i].channel);
            if (touch_statue[i] == 0)
            {
                hold_time[i] = hold_time[i] + 1;
            }
            else
            {

                hold_time[i] = 0;
            }
        }
        if (hold_time[2] >= 2)
        {
            static uint8_t count = 0;
            count = (count + 1) % 2;
            if (count == 1 && dlight.target > 0 && dlight.target < 100)
            {
                dlight.target++;
                update = true;
            }
        }
        if (hold_time[1] == 2)
        {
            dlight.target = (dlight.target == 0) ? 50 : 0;
            update = true;
        }
        if (hold_time[0] >= 2)
        {
            static uint8_t count = 0;
            count = (count + 1) % 2;
            if (count == 1 && dlight.target > 1)
            {
                dlight.target--;
                update = true;
            }
        }
        if (update == true && touch_statue[0] + touch_statue[1] + touch_statue[2] == 3)
        {
            update = false;
            dlight_csro_3t2scr_mqtt_update();
        }
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void dlight_csro_3t3scr_scr_task(void *args)
{
    while (true)
    {
        if (dlight.value < dlight.target)
        {
            dlight.value++;
        }
        else if (dlight.value > dlight.target)
        {
            dlight.value--;
        }
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void csro_dlight_csro_3t3scr_init(void)
{
    // config output gpio
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // config input gpio
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // config interupt gpio
    io_conf.intr_type = GPIO_PIN_INTR_NEGEDGE;
    io_conf.pin_bit_mask = GPIO_INTR_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 1;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(AC_ZERO_NUM, gpio_isr_handler, (void *)AC_ZERO_NUM);

    // config led channel.
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
        .freq_hz = 5000,                      // frequency of PWM signal
        .speed_mode = LEDC_MODE,              // timer mode
        .timer_num = LEDC_TIMER               // timer index
    };
    ledc_timer_config(&ledc_timer);
    for (uint8_t i = 0; i < LEDC_TOTAL_NUM; i++)
    {
        ledc_channel_config(&ledc_channel[i]);
    }

    //config hardware timer
    timer_config_t config;
    config.divider = 80;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = TIMER_AUTORELOAD_DIS;
    timer_init(TIMER_GROUP_0, TIMER_0, &config);
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_isr_register(TIMER_GROUP_0, TIMER_0, timer_group_isr, (void *)TIMER_0, ESP_INTR_FLAG_SHARED, NULL);

    gpio_set_level(TOUCH_EN_NUM, 1);
    xTaskCreate(dlight_csro_3t3scr_touch_task, "dlight_csro_3t3scr_touch_task", 2048, NULL, configMAX_PRIORITIES - 6, NULL);
    xTaskCreate(dlight_csro_3t3scr_scr_task, "dlight_csro_3t3scr_scr_task", 2048, NULL, configMAX_PRIORITIES - 7, NULL);
}
void csro_dlight_csro_3t3scr_on_connect(esp_mqtt_event_handle_t event)
{
    sprintf(mqttinfo.sub_topic, "csro/%s/%s/set/#", sysinfo.mac_str, sysinfo.dev_type);
    esp_mqtt_client_subscribe(event->client, mqttinfo.sub_topic, 0);

    char prefix[50], name[50], deviceid[50];
    sprintf(mqttinfo.pub_topic, "csro/light/%s_%s/config", sysinfo.mac_str, sysinfo.dev_type);
    sprintf(prefix, "csro/%s/%s", sysinfo.mac_str, sysinfo.dev_type);
    sprintf(name, "%s_%s_0", sysinfo.mac_str, sysinfo.dev_type);
    sprintf(deviceid, "%s_%s", sysinfo.mac_str, sysinfo.dev_type);

    cJSON *config_json = cJSON_CreateObject();
    cJSON *device = NULL;
    cJSON_AddStringToObject(config_json, "~", prefix);
    cJSON_AddStringToObject(config_json, "name", name);
    cJSON_AddStringToObject(config_json, "unique_id", name);
    cJSON_AddStringToObject(config_json, "cmd_t", "~/set");
    cJSON_AddStringToObject(config_json, "bri_cmd_t", "~/set/bright");
    cJSON_AddNumberToObject(config_json, "bri_scl", 100);
    cJSON_AddStringToObject(config_json, "bri_stat_t", "~/state");
    cJSON_AddStringToObject(config_json, "bri_val_tpl", "{{value_json.state.bright}}");
    cJSON_AddStringToObject(config_json, "on_cmd_type", "brightness");
    cJSON_AddNumberToObject(config_json, "pl_on", 1);
    cJSON_AddNumberToObject(config_json, "pl_off", 0);
    cJSON_AddStringToObject(config_json, "stat_t", "~/state");
    cJSON_AddStringToObject(config_json, "stat_val_tpl", "{{value_json.state.on}}");
    cJSON_AddStringToObject(config_json, "avty_t", "~/available");
    cJSON_AddStringToObject(config_json, "opt", "false");
    cJSON_AddItemToObject(config_json, "dev", device = cJSON_CreateObject());
    cJSON_AddStringToObject(device, "ids", deviceid);
    cJSON_AddStringToObject(device, "name", deviceid);
    cJSON_AddStringToObject(device, "mf", MANUFACTURER);
    cJSON_AddStringToObject(device, "mdl", "DLIGHT_CSRO_3T3SCR");
    cJSON_AddStringToObject(device, "sw", SOFT_VERSION);

    char *out = cJSON_PrintUnformatted(config_json);
    strcpy(mqttinfo.content, out);
    free(out);
    cJSON_Delete(config_json);
    esp_mqtt_client_publish(event->client, mqttinfo.pub_topic, mqttinfo.content, 0, 0, 1);

    sprintf(mqttinfo.pub_topic, "csro/%s/%s/available", sysinfo.mac_str, sysinfo.dev_type);
    esp_mqtt_client_publish(event->client, mqttinfo.pub_topic, "online", 0, 0, 1);
    dlight_csro_3t2scr_mqtt_update();
}
void csro_dlight_csro_3t3scr_on_message(esp_mqtt_event_handle_t event)
{

    char topic[80];
    char command[10];
    sprintf(topic, "csro/%s/%s/set", sysinfo.mac_str, sysinfo.dev_type);
    if (strncmp(topic, event->topic, event->topic_len) == 0)
    {
        if (strncmp("1", event->data, event->data_len) == 0)
        {
            dlight.target = 100;
        }
        else if (strncmp("0", event->data, event->data_len) == 0)
        {
            dlight.target = 0;
        }
    }
    sprintf(topic, "csro/%s/%s/set/bright", sysinfo.mac_str, sysinfo.dev_type);
    if (strncmp(topic, event->topic, event->topic_len) == 0)
    {
        memset(command, 0, 10);
        strncpy(command, event->data, event->data_len);
        int brightness = atoi(command);
        printf("%s, %d\r\n", command, brightness);
        if (brightness >= 0 && brightness <= 100)
        {
            dlight.target = brightness;
        }
    }
    dlight_csro_3t2scr_mqtt_update();
}

#endif