#ifndef MOSQUITTO_STUB_H
#define MOSQUITTO_STUB_H

#include <stdbool.h>
#include <stdint.h>

typedef struct mosquitto mosquitto;
struct mosquitto_message;

typedef void (*mosquitto_connect_callback)(struct mosquitto *, void *, int);
typedef void (*mosquitto_disconnect_callback)(struct mosquitto *, void *, int);
typedef void (*mosquitto_message_callback)(struct mosquitto *, void *, const struct mosquitto_message *);

struct mosquitto_message {
    int mid;
    char *topic;
    void *payload;
    int payloadlen;
    int qos;
    bool retain;
};

#define MOSQ_ERR_SUCCESS 0

mosquitto *mosquitto_new(const char *id, bool clean_start, void *userdata);
void mosquitto_destroy(mosquitto *mosq);
void mosquitto_lib_init(void);
void mosquitto_lib_cleanup(void);
int mosquitto_connect_async(struct mosquitto *mosq, const char *host, int port, int keepalive);
int mosquitto_loop(struct mosquitto *mosq, int timeout_ms, int max_packets);
int mosquitto_subscribe(struct mosquitto *mosq, int *mid, const char *sub, int qos);
int mosquitto_publish(struct mosquitto *mosq, int *mid, const char *topic, int payloadlen, const void *payload, int qos, bool retain);
int mosquitto_reconnect_delay_set(struct mosquitto *mosq, unsigned int reconnect_delay, unsigned int reconnect_delay_max, bool reconnect_exponential_backoff);
void mosquitto_connect_callback_set(struct mosquitto *mosq, mosquitto_connect_callback on_connect);
void mosquitto_disconnect_callback_set(struct mosquitto *mosq, mosquitto_disconnect_callback on_disconnect);
void mosquitto_message_callback_set(struct mosquitto *mosq, mosquitto_message_callback on_message);
const char *mosquitto_strerror(int rc);

#endif
