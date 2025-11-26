#include "simulated_sensors.h"

#include <cmath>
#include <algorithm>

void generate_simulated_sensors(double& temperature, double& humidity, uint8_t& pwm_aplicado, int current_pwm)
{
    static int counter = 0;
    counter++;

    temperature = 25.0 + 5.0 * std::sin(counter * 0.1);
    humidity = 55.0 + 15.0 * std::cos(counter * 0.07);

    int variation = static_cast<int>(3 * std::sin(counter * 0.2));
    int applied = std::clamp(current_pwm + variation, 0, 100);
    pwm_aplicado = static_cast<uint8_t>(applied);
}
