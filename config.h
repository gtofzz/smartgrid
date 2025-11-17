#ifndef CONFIG_H
#define CONFIG_H

/************* MQTT / TÓPICOS *************/
#define BROKER_IP           "192.168.106.11"
#define BROKER_PORT         1883
#define TOPIC_STATE         "grid/state"
#define TOPIC_CMD_EPOCH     "grid/cmd/epoch"
#define TOPIC_CMD_SHEDDING  "grid/cmd/shedding"

/************* LIMITES / TEMPOS *************/
#ifndef MAX_HOUSES
#define MAX_HOUSES          128
#endif

#define PUB_PERIOD_SEC      1
#define CTRL_PERIOD_SEC     1
#define BASE_CAP            4
#define NODE_TTL_SEC        (3*PUB_PERIOD_SEC + 1)

/************* HARDWARE (debounce) *************/
#define BTN_POLL_MS         10
#define BTN_STABLE_TICKS    5   /* 5*10ms = 50ms */

/* LED de rede: 0=verde 1=amarelo 2=vermelho */
#define NETLED_GREEN 0
#define NETLED_YELL  1
#define NETLED_RED   2

#endif
