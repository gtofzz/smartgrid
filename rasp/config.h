#pragma once

// MQTT broker configuration
#define BROKER_HOST "localhost"
#define BROKER_PORT 1883

// Group identification
#define GROUP_ID_NUM 3
#define GROUP_ID_STR "3"

// MQTT topics
#define TOPIC_PWM "grupo3/pwm"
#define TOPIC_SENSORS "grupo3/sensores"
#define TOPIC_STATUS "grupo3/status"

// Operating mode
#define USE_SIMULATED_SENSORS 1

// I2C configuration
#define I2C_DEVICE "/dev/i2c-1"
#define I2C_BASE_ADDR 0x28
#define I2C_SLAVE_ADDR (I2C_BASE_ADDR + GROUP_ID_NUM)
