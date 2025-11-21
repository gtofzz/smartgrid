#include "mqtt_if.h"
#include "cmdq.h"
#include "config.h"
#include "state.h"
#include "logbuf.h"

#ifdef MQTT_STUB
#include "mosquitto_stub.h"
#else
#include <mosquitto.h>
#endif

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

static struct mosquitto *g_client = NULL;
static char *g_client_id = NULL;
static pthread_mutex_t g_pub_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile int g_force_pub = 0;

static char *make_client_id(const char *prefix) {
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "%s_%ld", prefix, (long)getpid());
    return strdup(tmp);
}

static void mqtt_publish_str(const char *topic, const char *payload) {
    if (!g_client || !payload) return;
    pthread_mutex_lock(&g_pub_lock);
    int rc = mosquitto_publish(g_client, NULL, topic, (int)strlen(payload), payload, 1, false);
    pthread_mutex_unlock(&g_pub_lock);
    if (rc != MOSQ_ERR_SUCCESS) log_push("[MQTT] falha ao publicar %s: %s", topic, mosquitto_strerror(rc));
}

static void publish_status(void) {
    DeviceState st = state_get();
    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"Duty\":%d,\"TempC\":%.2f,\"Humid\":%.2f,\"PWMfb\":%d,\"I2CErr\":%d,\"timestamp\":%d}",
             st.duty_setpoint, st.last_sample.temp_cC/100.0, st.last_sample.hum_cP/100.0,
             st.last_sample.pwm_feedback, st.i2c_errors, now_ts());
    mqtt_publish_str(TOPIC_STATUS, payload);
}

static void publish_sensors(void) {
    DeviceState st = state_get();
    char payload[192];
    snprintf(payload, sizeof(payload),
             "{\"TempC\":%.2f,\"Humid\":%.2f,\"PWMfb\":%d}",
             st.last_sample.temp_cC/100.0, st.last_sample.hum_cP/100.0, st.last_sample.pwm_feedback);
    mqtt_publish_str(TOPIC_SENSORES, payload);
}

void mqtt_request_status_publish(void) { g_force_pub = 1; }

static void on_connect_cb(struct mosquitto *mosq, void *userdata, int rc) {
    if (rc == MOSQ_ERR_SUCCESS) {
        mosquitto_subscribe(mosq, NULL, TOPIC_CMD_LUZ, 1);
        log_push("[MQTT] conectado ao broker e inscrito em %s", TOPIC_CMD_LUZ);
    } else {
        log_push("[MQTT] falha de conexão: %d", rc);
    }
}

static void on_disconnect_cb(struct mosquitto *mosq, void *userdata, int rc) {
    log_push("[MQTT] desconectado (rc=%d), tentando reconectar", rc);
}

static void on_message_cb(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {
    if (!msg || !msg->payload || msg->payloadlen <= 0) return;
    if (strcmp(msg->topic, TOPIC_CMD_LUZ) != 0) return;

    int val = atoi((const char*)msg->payload);
    const char *p = strstr((const char*)msg->payload, "Duty");
    if (p) {
        /* tentativa simples de extrair Duty:<num> */
        val = atoi(p + 4);
    }
    val = clampi(val, PWM_MIN, PWM_MAX);
    cmdq_push(CMD_SET_PWM, val);
    log_push("[MQTT] comando PWM recebido: %d%%", val);
    mqtt_request_status_publish();
}

static void *mqtt_thread(void *arg) {
    (void)arg;
    g_client_id = make_client_id("raspi");
    g_client = mosquitto_new(g_client_id, true, NULL);
    if (!g_client) { log_push("[MQTT] erro ao criar cliente"); return NULL; }

    mosquitto_connect_callback_set(g_client, on_connect_cb);
    mosquitto_disconnect_callback_set(g_client, on_disconnect_cb);
    mosquitto_message_callback_set(g_client, on_message_cb);
    mosquitto_reconnect_delay_set(g_client, 1, 5, true);
    mosquitto_connect_async(g_client, BROKER_IP, BROKER_PORT, 30);

    time_t last_status = 0;
    while (g_run) {
        mosquitto_loop(g_client, 100, 1);
        time_t now = time(NULL);
        if (g_force_pub || (now - last_status) >= STATUS_PERIOD_SEC) {
            publish_status();
            publish_sensors();
            last_status = now;
            g_force_pub = 0;
        }
    }
    return NULL;
}

int mqtt_start_threads(void) {
    mosquitto_lib_init();
    pthread_t th;
    if (pthread_create(&th, NULL, mqtt_thread, NULL) != 0) return -1;
    pthread_detach(th);
    return 0;
}

void mqtt_join_threads(void) {
    if (g_client) { mosquitto_destroy(g_client); g_client = NULL; }
    if (g_client_id) free(g_client_id);
    mosquitto_lib_cleanup();
}
