#include "csro_common.h"

csro_system sysinfo;
csro_mqtt mqttinfo;
esp_mqtt_client_handle_t mqttclient;

void csro_main(void)
{
    nvs_handle handle;
    nvs_flash_init();
    nvs_open("system", NVS_READWRITE, &handle);
    nvs_get_u32(handle, "power_cnt", &sysinfo.power_cnt);
    nvs_set_u32(handle, "power_cnt", (sysinfo.power_cnt + 1));
    nvs_get_u8(handle, "router_flag", &sysinfo.router_flag);
    nvs_commit(handle);
    nvs_close(handle);
    printf("Power count %d.\r\n", sysinfo.power_cnt);
    if (sysinfo.router_flag == 1)
    {
        printf("starting mqtt......\r\n");
    }
    else
    {
        printf("starting smart......\r\n");
    }
}