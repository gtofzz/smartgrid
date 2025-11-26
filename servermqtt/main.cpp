#include <mosquitto.h>
#include <csignal>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

#include "config.h"

static std::atomic<bool> running(true);
static std::atomic<bool> connected(false);
static std::mutex data_mutex;
static double last_temperature = 0.0;
static double last_humidity = 0.0;

bool parse_double_field(const std::string &payload, const std::string &key, double &value)
{
    std::size_t pos = payload.find(key);
    if (pos == std::string::npos)
        return false;
    pos = payload.find(':', pos);
    if (pos == std::string::npos)
        return false;
    ++pos;
    try
    {
        value = std::stod(payload.substr(pos));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool parse_int_field(const std::string &payload, const std::string &key, int &value)
{
    std::size_t pos = payload.find(key);
    if (pos == std::string::npos)
        return false;
    pos = payload.find(':', pos);
    if (pos == std::string::npos)
        return false;
    ++pos;
    try
    {
        value = std::stoi(payload.substr(pos));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void handle_signal(int)
{
    running = false;
}

void on_connect(struct mosquitto *, void *, int rc)
{
    if (rc == 0)
    {
        connected = true;
        std::cout << "Conectado ao broker MQTT" << std::endl;
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
    std::cerr << "Desconectado do broker MQTT" << std::endl;
}

void on_message(struct mosquitto *, void *, const struct mosquitto_message *msg)
{
    if (!msg || !msg->topic || !msg->payload)
        return;

    std::string topic(msg->topic);
    std::string payload(static_cast<char *>(msg->payload), msg->payloadlen);

    if (topic == TOPIC_SENSORS)
    {
        double temp = 0.0;
        double hum = 0.0;
        int pwm_aplicado = 0;

        bool ok_temp = parse_double_field(payload, "temperatura", temp);
        bool ok_hum = parse_double_field(payload, "umidade", hum);
        bool ok_pwm = parse_int_field(payload, "pwm_aplicado", pwm_aplicado);

        if (ok_temp && ok_hum && ok_pwm)
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            last_temperature = temp;
            last_humidity = hum;
        }
        std::cout << "[SENSORES] " << payload << std::endl;
    }
    else if (topic == TOPIC_STATUS)
    {
        std::cout << "[STATUS] " << payload << std::endl;
    }
}

void pwm_input_loop(mosquitto *client)
{
    while (running)
    {
        std::string line;
        if (!std::getline(std::cin, line))
        {
            running = false;
            break;
        }
        try
        {
            int pwm = std::stoi(line);
            if (pwm < 0 || pwm > 100)
            {
                std::cerr << "Valor de PWM invalido (0-100)." << std::endl;
                continue;
            }
            mosquitto_publish(client, nullptr, TOPIC_PWM, line.size(), line.c_str(), 1, false);
        }
        catch (...)
        {
            std::cerr << "Entrada invalida, informe um inteiro." << std::endl;
        }
    }
}

int main()
{
    std::signal(SIGINT, handle_signal);

    mosquitto_lib_init();
    mosquitto *client = mosquitto_new(nullptr, true, nullptr);
    if (!client)
    {
        std::cerr << "Falha ao criar cliente mosquitto" << std::endl;
        mosquitto_lib_cleanup();
        return 1;
    }

    mosquitto_connect_callback_set(client, on_connect);
    mosquitto_disconnect_callback_set(client, on_disconnect);
    mosquitto_message_callback_set(client, on_message);

    int rc = mosquitto_connect(client, BROKER_HOST, BROKER_PORT, 60);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "Nao foi possivel conectar: " << mosquitto_strerror(rc) << std::endl;
    }

    mosquitto_subscribe(client, nullptr, TOPIC_SENSORS, 1);
    mosquitto_subscribe(client, nullptr, TOPIC_STATUS, 1);

    mosquitto_loop_start(client);

    std::thread input_thread(pwm_input_loop, client);

    while (running)
    {
        if (!connected)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            if (!running)
                break;
            int reconnect_rc = mosquitto_reconnect(client);
            if (reconnect_rc == MOSQ_ERR_SUCCESS)
            {
                mosquitto_subscribe(client, nullptr, TOPIC_SENSORS, 1);
                mosquitto_subscribe(client, nullptr, TOPIC_STATUS, 1);
            }
        }

        {
            std::lock_guard<std::mutex> lock(data_mutex);
            std::cout << "grupo " << GROUP_ID_STR << ", umidade " << last_humidity
                      << ", temperatura " << last_temperature << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    mosquitto_loop_stop(client, true);
    mosquitto_disconnect(client);

    if (input_thread.joinable())
        input_thread.join();

    mosquitto_destroy(client);
    mosquitto_lib_cleanup();

    return 0;
}
