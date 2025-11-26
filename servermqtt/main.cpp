#include <mosquitto.h>
#include <csignal>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <cstdio>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"

static std::atomic<bool> running(true);
static std::atomic<bool> connected(false);
static std::mutex data_mutex;
static double last_temperature = 0.0;
static double last_humidity = 0.0;
static pid_t broker_pid = -1;

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

std::string get_env_or_default(const char *name, const std::string &default_value)
{
    const char *value = std::getenv(name);
    if (value && *value)
        return value;
    return default_value;
}

bool parse_port(const std::string &value, int &port)
{
    try
    {
        long parsed = std::stol(value);
        if (parsed > 0 && parsed <= std::numeric_limits<uint16_t>::max())
        {
            port = static_cast<int>(parsed);
            return true;
        }
    }
    catch (...)
    {
    }
    return false;
}

void apply_overrides(int argc, char **argv, std::string &broker_host, int &broker_port)
{
    broker_host = get_env_or_default("MQTT_HOST", broker_host);

    const std::string env_port = get_env_or_default("MQTT_PORT", "");
    int parsed_port = broker_port;
    if (!env_port.empty() && !parse_port(env_port, parsed_port))
    {
        std::cerr << "Ignorando MQTT_PORT invalido: " << env_port << std::endl;
    }
    else if (!env_port.empty())
    {
        broker_port = parsed_port;
    }

    for (int i = 1; i < argc; ++i)
    {
        std::string arg(argv[i]);
        if (arg.rfind("--host=", 0) == 0)
        {
            broker_host = arg.substr(7);
        }
        else if (arg.rfind("--port=", 0) == 0)
        {
            if (!parse_port(arg.substr(7), broker_port))
            {
                std::cerr << "Valor de porta invalido em --port: " << arg.substr(7) << std::endl;
            }
        }
    }
}

bool should_start_embedded_broker(const std::string &host)
{
    return host == "localhost" || host == "127.0.0.1";
}

bool start_embedded_broker(int port)
{
    if (broker_pid > 0)
        return true;

    pid_t pid = fork();
    if (pid < 0)
    {
        std::perror("Erro ao criar processo do broker MQTT");
        return false;
    }

    if (pid == 0)
    {
        std::string port_str = std::to_string(port);
        execlp("mosquitto", "mosquitto", "-p", port_str.c_str(), nullptr);
        std::perror("Falha ao iniciar broker mosquitto");
        _exit(127);
    }

    broker_pid = pid;
    std::cout << "Broker MQTT incorporado iniciado na porta " << port << " (pid " << broker_pid << ")" << std::endl;
    return true;
}

void stop_embedded_broker()
{
    if (broker_pid > 0)
    {
        kill(broker_pid, SIGTERM);
        int status = 0;
        waitpid(broker_pid, &status, 0);
        std::cout << "Broker MQTT incorporado finalizado" << std::endl;
        broker_pid = -1;
    }
}

int main(int argc, char *argv[])
{
    std::signal(SIGINT, handle_signal);

    std::string broker_host = BROKER_HOST;
    int broker_port = BROKER_PORT;
    apply_overrides(argc, argv, broker_host, broker_port);

    if (should_start_embedded_broker(broker_host))
    {
        if (!start_embedded_broker(broker_port))
        {
            std::cerr << "Nao foi possivel iniciar o broker MQTT local" << std::endl;
            return 1;
        }
    }

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

    std::cout << "Conectando ao broker MQTT em " << broker_host << ':' << broker_port << std::endl;

    int rc = mosquitto_connect(client, broker_host.c_str(), broker_port, 60);
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

    stop_embedded_broker();

    return 0;
}
