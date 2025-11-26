#pragma once

#include <cstdint>
#include <string>

bool i2c_init(const char* device, int slave_addr, int& fd);
bool i2c_send_pwm(int fd, uint8_t pwm_value, std::string& error_msg);
bool i2c_read_sensors(int fd, double& temperature, double& humidity, uint8_t& pwm_aplicado, std::string& error_msg);
