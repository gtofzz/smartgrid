#ifndef MQTT_IF_H
#define MQTT_IF_H
#include <mosquitto.h>

int  mqtt_start_threads(void);
void mqtt_join_threads(void);

/* Publicadores úteis */
void mqtt_publish_state_now(void);
void mqtt_publish_shedding_cmd(int epoch, int targetOff);

#endif
