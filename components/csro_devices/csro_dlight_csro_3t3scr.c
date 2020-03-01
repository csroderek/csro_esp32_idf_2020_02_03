#include "csro_devices.h"

#ifdef DLIGHT_CSRO_3T3SCR

#define TOUCH_01_NUM GPIO_NUM_4
#define TOUCH_02_NUM GPIO_NUM_2
#define TOUCH_03_NUM GPIO_NUM_15

#define AC_ZERO_NUM GPIO_NUM_26
#define SCR_01_NUM GPIO_NUM_27
#define SCR_02_NUM GPIO_NUM_14
#define SCR_03_NUM GPIO_NUM_12
#define TOUCH_EN_NUM GPIO_NUM_16

#define GPIO_INPUT_PIN_SEL ((1ULL << TOUCH_01_NUM) | (1ULL << TOUCH_02_NUM) | (1ULL << TOUCH_03_NUM))
#define GPIO_OUTPUT_PIN_SEL ((1ULL << SCR_01_NUM) | (1ULL << SCR_02_NUM) | (1ULL << SCR_03_NUM) | (1ULL << TOUCH_EN_NUM))
#define GPIO_INTR_PIN_SEL (1ULL << AC_ZERO_NUM)

#define LEDC_TOTAL_NUM 3
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CH0_GPIO 5
#define LEDC_CH0_CHANNEL LEDC_CHANNEL_0
#define LEDC_CH1_GPIO 18
#define LEDC_CH1_CHANNEL LEDC_CHANNEL_1
#define LEDC_CH2_GPIO 19
#define LEDC_CH2_CHANNEL LEDC_CHANNEL_2

uint16_t bright_index[100] = {
    732, 725, 717, 710, 703, 696, 689, 683, 676, 670,
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

static void IRAM_ATTR timer_group_isr(void *para)
{
    static bool pulse_timer = false;
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

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, bright_index[dlight.value] * 10 - 800);
    timer_start(TIMER_GROUP_0, TIMER_0);
}

static void dlight_csro_3t3scr_touch_task(void *args)
{
    static uint16_t hold_time[3];
    while (true)
    {
        uint8_t touch_statue[3] = {gpio_get_level(TOUCH_01_NUM), gpio_get_level(TOUCH_02_NUM), gpio_get_level(TOUCH_03_NUM)};
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
        if (hold_time[0] >= 2)
        {
            static uint8_t count = 0;
            count = (count + 1) % 2;
            if (count == 1 && dlight.target > 0 && dlight.target < 99)
            {
                dlight.target++;
            }
        }
        if (hold_time[1] == 2)
        {
            dlight.target = (dlight.target == 0) ? 50 : 0;
        }
        if (hold_time[2] >= 2)
        {
            static uint8_t count = 0;
            count = (count + 1) % 2;
            if (count == 1 && dlight.target > 0 && dlight.target > 1)
            {
                dlight.target--;
            }
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
    timer_isr_register(TIMER_GROUP_0, TIMER_0, timer_group_isr, (void *)TIMER_0, ESP_INTR_FLAG_IRAM, NULL);

    gpio_set_level(TOUCH_EN_NUM, 1);
    xTaskCreate(dlight_csro_3t3scr_touch_task, "dlight_csro_3t3scr_touch_task", 2048, NULL, configMAX_PRIORITIES - 6, NULL);
    xTaskCreate(dlight_csro_3t3scr_scr_task, "dlight_csro_3t3scr_scr_task", 2048, NULL, configMAX_PRIORITIES - 7, NULL);
}
void csro_dlight_csro_3t3scr_on_connect(esp_mqtt_event_handle_t event)
{
}
void csro_dlight_csro_3t3scr_on_message(esp_mqtt_event_handle_t event)
{
}

#endif