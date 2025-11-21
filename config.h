#ifndef CONFIG_H
#define CONFIG_H

/* ===== MQTT ===== */
#define BROKER_IP           "127.0.0.1"
#define BROKER_PORT         1883

/* Tópicos padronizados segundo o enunciado */
#define TOPIC_CMD_LUZ       "cmd/luz"       /* Web -> Raspi */
#define TOPIC_STATUS        "cmd/status"    /* Raspi -> Web */
#define TOPIC_SENSORES      "cmd/sensores"  /* Raspi -> Web */

/* Periodicidades (segundos) */
#define STATUS_PERIOD_SEC   2
#define SENSOR_PERIOD_SEC   1

/* ===== I2C ===== */
#define I2C_DEVICE          "/dev/i2c-1"
#define I2C_ADDRESS         0x28

/* Formato STM32 -> Raspi (t1/100, RH/100, pwm%) */
#define I2C_FRAME_LEN       5

/* ===== PWM ===== */
#define PWM_MIN             0
#define PWM_MAX             100

#endif
