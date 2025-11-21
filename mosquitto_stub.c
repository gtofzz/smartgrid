#include "mosquitto_stub.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mosquitto {
    mosquitto_connect_callback on_connect;
    mosquitto_disconnect_callback on_disconnect;
    mosquitto_message_callback on_message;
};

mosquitto *mosquitto_new(const char *id, bool clean_start, void *userdata) {
    (void)id; (void)clean_start; (void)userdata;
    struct mosquitto *m = calloc(1, sizeof(struct mosquitto));
    return m;
}

void mosquitto_destroy(mosquitto *mosq) {
    free(mosq);
}

void mosquitto_lib_init(void) {}
void mosquitto_lib_cleanup(void) {}

int mosquitto_connect_async(struct mosquitto *mosq, const char *host, int port, int keepalive) {
    (void)mosq; (void)host; (void)port; (void)keepalive; return MOSQ_ERR_SUCCESS;
}

int mosquitto_loop(struct mosquitto *mosq, int timeout_ms, int max_packets) {
    (void)mosq; (void)timeout_ms; (void)max_packets; return MOSQ_ERR_SUCCESS;
}

int mosquitto_subscribe(struct mosquitto *mosq, int *mid, const char *sub, int qos) {
    (void)mosq; (void)mid; (void)sub; (void)qos; return MOSQ_ERR_SUCCESS;
}

int mosquitto_publish(struct mosquitto *mosq, int *mid, const char *topic, int payloadlen, const void *payload, int qos, bool retain) {
    (void)mosq; (void)mid; (void)topic; (void)payloadlen; (void)payload; (void)qos; (void)retain; return MOSQ_ERR_SUCCESS;
}

int mosquitto_reconnect_delay_set(struct mosquitto *mosq, unsigned int reconnect_delay, unsigned int reconnect_delay_max, bool reconnect_exponential_backoff) {
    (void)mosq; (void)reconnect_delay; (void)reconnect_delay_max; (void)reconnect_exponential_backoff; return MOSQ_ERR_SUCCESS;
}

void mosquitto_connect_callback_set(struct mosquitto *mosq, mosquitto_connect_callback on_connect) {
    if (mosq) mosq->on_connect = on_connect;
}

void mosquitto_disconnect_callback_set(struct mosquitto *mosq, mosquitto_disconnect_callback on_disconnect) {
    if (mosq) mosq->on_disconnect = on_disconnect;
}

void mosquitto_message_callback_set(struct mosquitto *mosq, mosquitto_message_callback on_message) {
    if (mosq) mosq->on_message = on_message;
}

const char *mosquitto_strerror(int rc) {
    switch (rc) {
        case MOSQ_ERR_SUCCESS: return "success";
        default: return "stub";
    }
}
