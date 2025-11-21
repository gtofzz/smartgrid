#include "hw_io.h"
#include "config.h"
#include "logbuf.h"
#include "state.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int g_i2c_fd = -1;

int hw_init(void) {
    g_i2c_fd = open(I2C_DEVICE, O_RDWR);
    if (g_i2c_fd < 0) {
        log_push("[I2C] falha ao abrir %s: %s", I2C_DEVICE, strerror(errno));
        return -1;
    }
    if (ioctl(g_i2c_fd, I2C_SLAVE, I2C_ADDRESS) < 0) {
        log_push("[I2C] ioctl I2C_SLAVE falhou: %s", strerror(errno));
        close(g_i2c_fd);
        g_i2c_fd = -1;
        return -1;
    }
    log_push("[I2C] conectado no endereço 0x%02X via %s", I2C_ADDRESS, I2C_DEVICE);
    return 0;
}

int hw_send_pwm(int duty) {
    if (g_i2c_fd < 0) return -1;
    uint8_t frame[2];
    frame[0] = 0x01;             /* tipo de mensagem: atualizar duty */
    frame[1] = (uint8_t)duty;    /* 0-100 */
    ssize_t w = write(g_i2c_fd, frame, sizeof(frame));
    if (w != (ssize_t)sizeof(frame)) {
        log_push("[I2C] write PWM falhou (%zd/%zu)", w, sizeof(frame));
        return -1;
    }
    return 0;
}

int hw_read_sample(SensorSample *out) {
    if (!out || g_i2c_fd < 0) return -1;
    uint8_t buf[I2C_FRAME_LEN] = {0};
    ssize_t r = read(g_i2c_fd, buf, sizeof(buf));
    if (r != (ssize_t)sizeof(buf)) {
        log_push("[I2C] leitura incompleta (%zd/%zu)", r, sizeof(buf));
        return -1;
    }

    /* Formato: temp(int16), umidade(int16), pwm(uint8) */
    int16_t temp = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t hum  = (int16_t)((buf[2] << 8) | buf[3]);
    out->temp_cC = temp;
    out->hum_cP = hum;
    out->pwm_feedback = buf[4];
    return 0;
}

void hw_shutdown(void) {
    if (g_i2c_fd >= 0) {
        close(g_i2c_fd);
        g_i2c_fd = -1;
    }
}
