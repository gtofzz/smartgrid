#include "mqtt_if.h"
#include "config.h"
#include "types.h"
#include "state.h"
#include "logbuf.h"

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <cjson/cJSON.h>

static struct mosquitto *g_pub = NULL;
static struct mosquitto *g_sub = NULL;
static char *g_pub_id = NULL, *g_sub_id = NULL;

/* Helpers */
static char* make_client_id(const char *prefix, int id) {
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "%s_%d_%ld", prefix, id, (long)getpid());
    return strdup(tmp);
}
static void mqtt_publish_json(struct mosquitto *m, const char *topic, cJSON *json) {
    if (!m) return;
    char *payload = cJSON_PrintUnformatted(json);
    if (!payload) return;
    int rc = mosquitto_publish(m, NULL, topic, (int)strlen(payload), payload, /*qos*/1, /*retain*/false);
    if (rc != MOSQ_ERR_SUCCESS) log_push("[MQTT-PUB] falha ao publicar %s: %s", topic, mosquitto_strerror(rc));
    free(payload);
}

/* ========== PUBLICADORES ========== */
void mqtt_publish_state_now(void) {
    if (!g_pub) return;
    HouseState s = state_get_self();
    s.active = (s.c1 || s.c2);
    s.ts = now_ts();
    state_set_self(s);
    cache_set(&s);
    presence_touch(s.ID);

    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "ID",    s.ID);
    cJSON_AddNumberToObject(j, "c1",    s.c1);
    cJSON_AddNumberToObject(j, "c2",    s.c2);
    cJSON_AddNumberToObject(j, "solar", s.solar);
    cJSON_AddNumberToObject(j, "active",s.active);
    cJSON_AddNumberToObject(j, "shed",  s.shed);
    cJSON_AddNumberToObject(j, "noOffer",s.noOffer);
    cJSON_AddNumberToObject(j, "ts",    s.ts);

    mqtt_publish_json(g_pub, TOPIC_STATE, j);
    cJSON_Delete(j);
}

void mqtt_publish_shedding_cmd(int epoch, int targetOff) {
    if (!g_pub) return;
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "epoch", epoch);
    cJSON_AddNumberToObject(j, "targetOff", targetOff);
    mqtt_publish_json(g_pub, TOPIC_CMD_SHEDDING, j);
    cJSON_Delete(j);
}

/* ========== CALLBACKS ========== */
static void on_connect_cb(struct mosquitto *mosq, void *userdata, int rc) {
    if (rc == MOSQ_ERR_SUCCESS) {
        mosquitto_subscribe(mosq, NULL, TOPIC_STATE, 1);
        mosquitto_subscribe(mosq, NULL, TOPIC_CMD_EPOCH, 1);
        mosquitto_subscribe(mosq, NULL, TOPIC_CMD_SHEDDING, 1);
        log_push("[MQTT] conectado e inscrito.");
    } else {
        log_push("[MQTT] falha de conexão: %d", rc);
    }
}
static void on_disconnect_cb(struct mosquitto *mosq, void *userdata, int rc) {
    log_push("[MQTT] desconectado (rc=%d). Reconnecting…", rc);
}
static void on_message_cb(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {
    if (!msg || !msg->payload || msg->payloadlen <= 0) return;
    const char *topic = msg->topic ? msg->topic : "";
    cJSON *json = cJSON_Parse((const char*)msg->payload);
    if (!json) return;

    if (strcmp(topic, TOPIC_STATE) == 0) {
        HouseState h = {0};
        cJSON *id = cJSON_GetObjectItem(json, "ID");
        if (!id || !cJSON_IsNumber(id)) { cJSON_Delete(json); return; }
        h.ID = id->valueint;
        h.c1 = cJSON_GetObjectItem(json, "c1") ? cJSON_GetObjectItem(json, "c1")->valueint : 0;
        h.c2 = cJSON_GetObjectItem(json, "c2") ? cJSON_GetObjectItem(json, "c2")->valueint : 0;
        h.solar = cJSON_GetObjectItem(json, "solar") ? cJSON_GetObjectItem(json, "solar")->valueint : 0;
        h.active= (h.c1 || h.c2) ? 1 : 0;
        h.shed  = cJSON_GetObjectItem(json, "shed") ? cJSON_GetObjectItem(json, "shed")->valueint : 0;
        h.noOffer=cJSON_GetObjectItem(json, "noOffer") ? cJSON_GetObjectItem(json, "noOffer")->valueint : 0;
        h.ts    = cJSON_GetObjectItem(json, "ts") ? cJSON_GetObjectItem(json, "ts")->valueint : now_ts();
        cache_set(&h);
        presence_touch(h.ID);
    } else if (strcmp(topic, TOPIC_CMD_EPOCH) == 0) {
        cJSON *ep = cJSON_GetObjectItem(json, "epoch");
        if (ep && cJSON_IsNumber(ep)) {
            grid_set_epoch(ep->valueint);
            log_push("[EPOCH] atualizado para %d", ep->valueint);
        }
    } else if (strcmp(topic, TOPIC_CMD_SHEDDING) == 0) {
        cJSON *ep = cJSON_GetObjectItem(json, "epoch");
        cJSON *to = cJSON_GetObjectItem(json, "targetOff");
        if (ep && cJSON_IsNumber(ep) && to && cJSON_IsNumber(to)) {
            scmd_set(ep->valueint, to->valueint);
            log_push("[CMD] shedding epoch=%d targetOff=%d", ep->valueint, to->valueint);
        }
    }
    cJSON_Delete(json);
}

/* ========== THREADS ========== */
static void *mqtt_sub_thread(void *arg) {
    (void)arg;
    int id = state_get_self().ID;
    g_sub_id = make_client_id("Casa_sub", id);
    g_sub = mosquitto_new(g_sub_id, true, NULL);
    if (!g_sub) { log_push("[MQTT-SUB] erro ao criar cliente"); return NULL; }
    mosquitto_connect_callback_set(g_sub, on_connect_cb);
    mosquitto_message_callback_set(g_sub, on_message_cb);
    mosquitto_disconnect_callback_set(g_sub, on_disconnect_cb);
    mosquitto_reconnect_delay_set(g_sub, 1, 5, true);
    mosquitto_connect_async(g_sub, BROKER_IP, BROKER_PORT, 30);
    mosquitto_loop_forever(g_sub, -1, 1);
    return NULL;
}

static void *mqtt_pub_thread(void *arg) {
    (void)arg;
    int id = state_get_self().ID;
    g_pub_id = make_client_id("Casa_pub", id);
    g_pub = mosquitto_new(g_pub_id, true, NULL);
    if (!g_pub) { log_push("[MQTT-PUB] erro ao criar cliente"); return NULL; }
    mosquitto_disconnect_callback_set(g_pub, on_disconnect_cb);
    mosquitto_reconnect_delay_set(g_pub, 1, 5, true);
    mosquitto_connect_async(g_pub, BROKER_IP, BROKER_PORT, 30);

    time_t last_pub = 0;
    while (g_run) {
        mosquitto_loop(g_pub, 50, 1);
        time_t now = time(NULL);
        if (now - last_pub >= PUB_PERIOD_SEC) {
            mqtt_publish_state_now();
            last_pub = now;
        }
    }
    return NULL;
}

int mqtt_start_threads(void){
    mosquitto_lib_init();
    pthread_t th_sub, th_pub;
    if (pthread_create(&th_sub, NULL, mqtt_sub_thread, NULL)!=0) return -1;
    if (pthread_create(&th_pub, NULL, mqtt_pub_thread, NULL)!=0) return -1;
    pthread_detach(th_sub);
    pthread_detach(th_pub);
    return 0;
}

void mqtt_join_threads(void){
    if (g_pub) { mosquitto_destroy(g_pub); g_pub=NULL; }
    if (g_sub) { mosquitto_destroy(g_sub); g_sub=NULL; }
    if (g_pub_id) free(g_pub_id);
    if (g_sub_id) free(g_sub_id);
    mosquitto_lib_cleanup();
}
