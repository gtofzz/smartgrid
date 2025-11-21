#ifndef MQTT_IF_H
#define MQTT_IF_H

#ifdef MQTT_STUB
#include "mosquitto_stub.h"
#else
#include <mosquitto.h>
#endif

int mqtt_start_threads(void);
void mqtt_join_threads(void);
void mqtt_request_status_publish(void);

#endif
