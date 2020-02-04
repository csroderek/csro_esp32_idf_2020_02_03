#include "csro_devices.h"

void csro_device_init(void)
{
#ifdef MOTOR_CSRO_3T2R
    sprintf(sysinfo.dev_type, "motor_csro_3t2r");
    csro_motor_csro_3t2r_init();

#elif defined DLIGHT_CSRO_3T3SCR
    sprintf(sysinfo.dev_type, "dlight_csro_3t3scr");
    csro_dlight_csro_3t3scr_init();
#endif
}
void csro_device_on_connect(esp_mqtt_event_handle_t event)
{
#ifdef MOTOR_CSRO_3T2R
    csro_motor_csro_3t2r_on_connect(event);
#elif defined DLIGHT_CSRO_3T3SCR
    csro_dlight_csro_3t3scr_on_connect(event);
#endif
}
void csro_device_on_message(esp_mqtt_event_handle_t event)
{
#ifdef MOTOR_CSRO_3T2R
    csro_motor_csro_3t2r_on_message(event);
#elif defined DLIGHT_CSRO_3T3SCR
    csro_dlight_csro_3t3scr_on_message(event);
#endif
}