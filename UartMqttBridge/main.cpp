#include <stdio.h>
#include "MQTTClient.h"
#include "message_types.hpp"
#include <iostream>
#include <vector>
#include <tuple>
#include "ShmHandler.hpp"
#include "MQTTHandler.hpp"
#include "UartHandler.hpp"
#include "json.hpp"
#include <thread>
#include <chrono>

#define LOOP_SLEEP_MS 5
#define UPDATE_MISC_IT_MAX (250 / LOOP_SLEEP_MS)

void initializeMessages(std::vector<MessFromMQTT> *mess_from_mqtt, std::vector<MessFromUART> *mess_from_uart)
{
    // Initialize MQTT messages
    mess_from_mqtt->clear();
    for (int i = 0; i < MESSAGE_TYPE::FMQTT_MESSAGE_SIZE; ++i)
    {
        sem_t sem;
        pthread_mutex_t mutex;

        sem_init(&sem, 0, 0); // unnamed semaphore, initial value 0
        pthread_mutex_init(&mutex, nullptr);

        MESSAGE_TYPE::message msg{}; // default message struct

        mess_from_mqtt->emplace_back(sem, mutex, static_cast<MESSAGE_TYPE::FMQTT_MESSAGE_TYPE>(i), msg);
    }

    // Initialize UART messages
    mess_from_uart->clear();
    for (int i = 0; i < MESSAGE_TYPE::FUART_MESSAGE_SIZE; ++i)
    {
        sem_t sem;
        pthread_mutex_t mutex;

        sem_init(&sem, 0, 0);
        pthread_mutex_init(&mutex, nullptr);

        MESSAGE_TYPE::message msg{};

        mess_from_uart->emplace_back(sem, mutex, static_cast<MESSAGE_TYPE::FUART_MESSAGE_TYPE>(i), msg);
    }
}

void destroyMessages(std::vector<MessFromMQTT> *mess_from_mqtt, std::vector<MessFromUART> *mess_from_uart)
{
    for (auto &entry : *mess_from_mqtt)
    {
        sem_t &sem = std::get<0>(entry);
        pthread_mutex_t &mutex = std::get<1>(entry);

        sem_destroy(&sem);
        pthread_mutex_destroy(&mutex);
    }
    mess_from_mqtt->clear();

    for (auto &entry : *mess_from_uart)
    {
        sem_t &sem = std::get<0>(entry);
        pthread_mutex_t &mutex = std::get<1>(entry);

        sem_destroy(&sem);
        pthread_mutex_destroy(&mutex);
    }
    mess_from_uart->clear();
}

int main()
{
    initUart();
    uint64_t update_misc_it = 0;
    std::vector<MessFromMQTT> mess_from_MQTT;
    std::vector<MessFromUART> mess_from_UART;
    ShmHandler shmHandler;
    Misc_Info misc_info;
    initializeMessages(&mess_from_MQTT, &mess_from_UART);
    nlohmann::json j;
    // Example usage:
    std::cout << "Initialized " << mess_from_MQTT.size() << " MQTT messages\n";
    std::cout << "Initialized " << mess_from_UART.size() << " UART messages\n";
    MQTTHandler mqtt_hander(&mess_from_MQTT, &mess_from_UART);
    MqttMessage mqtt_message;
    while (true)
    {
        if (mqtt_hander.handleReceive(&mqtt_message) == 0)
        {
            if (mqtt_message.topic == "guider/camera_controls")
            {
                int gain, exp, inter = -1;
                nlohmann::json j = nlohmann::json::parse(mqtt_message.payload);
                if (j.contains("gain"))
                {
                    gain = static_cast<int>(j["gain"]);
                }
                if (j.contains("exp"))
                {
                    exp = static_cast<int>(j["exp"]);
                }
                if (j.contains("inter"))
                {
                    inter = static_cast<int>(j["inter"]);
                }
                std::cout << "setting props:" << gain << " " << exp << " " << inter << " " << std::endl;
                shmHandler.setupCameraGainExpoInterval(gain, exp, inter);
                ImageDataType dat_type;
                if (j.contains("data_type"))
                {
                    std::cout << "New data type:" << dat_type << std::endl;
                    dat_type = static_cast<ImageDataType>(j["data_type"]);
                    shmHandler.setupCameraDataType(dat_type);
                }
            }
        }

        if (update_misc_it % UPDATE_MISC_IT_MAX == 0)
        {
            shmHandler.readMiscInfo(misc_info);
            if (misc_info.updated == true)
            {
                std::string message = "{ \"curr_expo\": " + std::to_string(misc_info.current_exposure_time) +
                                      ", \"final_expo\": " + std::to_string(misc_info.final_exposure_time) + " }";
                mqtt_hander.publish("guider/exposure_status", message);
            }
        }

        update_misc_it += 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    // Destroy all semaphores and mutexes before exit
    destroyMessages(&mess_from_MQTT, &mess_from_UART);
    return 0;
}