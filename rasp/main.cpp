#include <mosquitto.h>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>
#include <unistd.h>

#include "config.h"
#include "i2c_helpers.h"
#include "simulated_sensors.h"

static std::atomic<bool> running(true);
static std::atomic<bool> connected(false);
static int current_pwm = 0;
static int i2c_failures = 0;
static mosquitto *client = nullptr;
static int i2c_fd = -1;

void handle_signal(int)
{
    running = false;
}

void publish_status(const std::string &msg)
{
    if (client)
    {
        mosquitto_publish(client, nullptr, TOPIC_STATUS, msg.size(), msg.c_str(), 1, false);
    }
}

void on_connect(struct mosquitto *mosq, void *, int rc)
{
    if (rc == 0)
    {
        connected = true;
        mosquitto_subscribe(mosq, nullptr, TOPIC_PWM, 1);
        std::cout << "RASP conectado ao broker MQTT" << std::endl;
    }
    else
    {
        connected = false;
        std::cerr << "Falha ao conectar ao broker, rc=" << rc << std::endl;
    }
}

void on_disconnect(struct mosquitto *, void *, int)
{
    connected = false;
    std::cerr << "RASP desconectado do broker MQTT" << std::endl;
}

void on_message(struct mosquitto *, void *, const struct mosquitto_message *msg)
{
    if (!msg || !msg->topic || !msg->payload)
        return;

    std::string topic(msg->topic);
    std::string payload(static_cast<char *>(msg->payload), msg->payloadlen);

    if (topic == TOPIC_PWM)
    {
        try
        {
            int pwm = std::stoi(payload);
            if (pwm < 0 || pwm > 100)
                return;
            std::cout << "MQTT: PWM recebido=" << pwm << std::endl;
            current_pwm = pwm;

            if (USE_SIMULATED_SENSORS == 0 && i2c_fd >= 0)
            {
                std::cout << "PWM (modo real) desejado para envio: " << current_pwm << std::endl;
                std::string error_msg;
                if (!i2c_send_pwm(i2c_fd, static_cast<uint8_t>(current_pwm), error_msg))
                {
                    publish_status(error_msg);
                }
            }
        }
        catch (...)
        {
            // ignore invalid payloads
        }
    }
}

int main()
{
    std::signal(SIGINT, handle_signal);

    if (USE_SIMULATED_SENSORS == 0)
    {
        if (!i2c_init(I2C_DEVICE, I2C_SLAVE_ADDR, i2c_fd))
        {
            std::cerr << "Falha ao inicializar I2C em " << I2C_DEVICE << std::endl;
            return 1;
        }
    }

    mosquitto_lib_init();
    client = mosquitto_new(nullptr, true, nullptr);
    if (!client)
    {
        std::cerr << "Nao foi possivel criar cliente MQTT" << std::endl;
        mosquitto_lib_cleanup();
        return 1;
    }

    mosquitto_connect_callback_set(client, on_connect);
    mosquitto_disconnect_callback_set(client, on_disconnect);
    mosquitto_message_callback_set(client, on_message);

    int rc = mosquitto_connect(client, BROKER_HOST, BROKER_PORT, 60);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "Erro ao conectar: " << mosquitto_strerror(rc) << std::endl;
    }

    mosquitto_loop_start(client);

    while (running)
    {
        if (!connected)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            if (!running)
                break;
            if (mosquitto_reconnect(client) == MOSQ_ERR_SUCCESS)
            {
                mosquitto_subscribe(client, nullptr, TOPIC_PWM, 1);
            }
        }

        double temperature = 0.0;
        double humidity = 0.0;
        uint8_t pwm_aplicado = 0;
        std::string error_msg;

        if (USE_SIMULATED_SENSORS == 0)
        {
            if (!i2c_read_sensors(i2c_fd, temperature, humidity, pwm_aplicado, error_msg))
            {
                ++i2c_failures;
                publish_status(error_msg);
                if (i2c_failures > 5)
                {
                    publish_status("Sub-nó inativo ou sem resposta");
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            else
            {
                i2c_failures = 0;
            }
        }
        else
        {
            generate_simulated_sensors(temperature, humidity, pwm_aplicado, current_pwm);
        }

        std::ostringstream oss;
        oss << "{\"grupo\":" << GROUP_ID_NUM
            << ",\"temperatura\":" << temperature
            << ",\"umidade\":" << humidity
            << ",\"pwm_aplicado\":" << static_cast<int>(pwm_aplicado)
            << "}";

        std::string payload = oss.str();
        mosquitto_publish(client, nullptr, TOPIC_SENSORS, payload.size(), payload.c_str(), 1, false);

        std::cout << "RASP: grupo " << GROUP_ID_STR
                  << ", PWM_solicitado=" << current_pwm
                  << ", PWM_aplicado=" << static_cast<int>(pwm_aplicado)
                  << ", TEMP=" << temperature
                  << ", HUM=" << humidity << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    mosquitto_loop_stop(client, true);
    mosquitto_disconnect(client);
    mosquitto_destroy(client);
    mosquitto_lib_cleanup();

    if (USE_SIMULATED_SENSORS == 0 && i2c_fd >= 0)
    {
        close(i2c_fd);
    }

    return 0;
}
