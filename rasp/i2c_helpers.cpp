#include "i2c_helpers.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include <cstdint>
#include <cerrno>
#include <cstring>
#include <string>
#include <chrono>
#include <thread>
#include <bitset>
#include <sstream>
#include <iostream>

namespace
{
    std::string bytes_to_bitstring(const uint8_t* data, size_t len)
    {
        std::ostringstream oss;
        for (size_t i = 0; i < len; ++i)
        {
            oss << "0b" << std::bitset<8>(data[i]);
            if (i + 1 < len)
            {
                oss << ' ';
            }
        }
        return oss.str();
    }
}

bool i2c_init(const char* device, int slave_addr, int& fd)
{
    fd = open(device, O_RDWR | O_NONBLOCK);
    if (fd < 0)
    {
        return false;
    }

    if (ioctl(fd, I2C_SLAVE, slave_addr) < 0)
    {
        close(fd);
        fd = -1;
        return false;
    }
    return true;
}

bool i2c_send_pwm(int fd, uint8_t pwm_value, std::string& error_msg)
{
    uint8_t buffer[2] = {0x01, pwm_value};
    std::cout << "[I2C DEBUG] Enviando PWM=" << static_cast<int>(pwm_value)
              << " bytes=" << bytes_to_bitstring(buffer, sizeof(buffer)) << std::endl;
    ssize_t written = write(fd, buffer, 2);
    if (written != 2)
    {
        if (errno == ENXIO)
        {
            error_msg = "Erro I2C: NACK na escrita";
        }
        else if (errno == 121)
        {
            error_msg = "Falha no barramento I2C, verifique conexões";
        }
        else
        {
            error_msg = "Falha ao escrever no barramento I2C";
        }
        return false;
    }
    return true;
}

bool i2c_read_sensors(int fd, double& temperature, double& humidity, uint8_t& pwm_aplicado, std::string& error_msg)
{
    uint8_t cmd = 0x02;
    std::cout << "[I2C DEBUG] Solicitando leitura com comando=" << bytes_to_bitstring(&cmd, 1) << std::endl;
    ssize_t w = write(fd, &cmd, 1);
    if (w != 1)
    {
        if (errno == ENXIO)
        {
            error_msg = "Erro I2C: NACK na escrita";
        }
        else if (errno == 121)
        {
            error_msg = "Falha no barramento I2C, verifique conexões";
        }
        else
        {
            error_msg = "Falha ao escrever no barramento I2C";
        }
        return false;
    }

    uint8_t buffer[5] = {0};
    size_t received = 0;
    auto start = std::chrono::steady_clock::now();

    while (received < sizeof(buffer))
    {
        ssize_t r = read(fd, buffer + received, sizeof(buffer) - received);
        if (r > 0)
        {
            received += static_cast<size_t>(r);
        }
        else if (r == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            else if (errno == 121)
            {
                error_msg = "Falha no barramento I2C, verifique conexões";
                return false;
            }
            else
            {
                error_msg = "Falha ao ler do barramento I2C";
                return false;
            }
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed_ms > 100)
        {
            error_msg = "Timeout na leitura I2C";
            return false;
        }
    }

    int16_t temp_x100 = static_cast<int16_t>((buffer[0] << 8) | buffer[1]);
    int16_t hum_x100  = static_cast<int16_t>((buffer[2] << 8) | buffer[3]);
    uint8_t pwm_aplic = buffer[4];

    std::cout << "[I2C DEBUG] Dados recebidos (brutos)="
              << bytes_to_bitstring(buffer, sizeof(buffer)) << std::endl;

    if (temp_x100 < -4000 || temp_x100 > 12500 || hum_x100 < 0 || hum_x100 > 10000 || pwm_aplic > 100)
    {
        error_msg = "Dados inválidos recebidos via I2C";
        return false;
    }

    temperature = temp_x100 / 100.0;
    humidity = hum_x100 / 100.0;
    pwm_aplicado = pwm_aplic;
    return true;
}
