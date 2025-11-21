#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <time.h>

typedef struct {
    int temp_cC;       /* temperatura em centésimos de °C */
    int hum_cP;        /* umidade relativa em centésimos de % */
    int pwm_feedback;  /* duty-cycle retornado pelo STM32 */
} SensorSample;

typedef struct {
    int duty_setpoint; /* último duty enviado ao STM32 */
    SensorSample last_sample;
    time_t last_sample_ts;
    int i2c_errors;
} DeviceState;

typedef enum {
    CMD_SET_PWM = 1,
    CMD_READ_SENSORS = 2,
    CMD_QUIT = 9
} CommandType;

typedef struct {
    CommandType type;
    int arg;
} Command;

#endif
