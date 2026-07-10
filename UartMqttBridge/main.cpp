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
#define UPDATE_CAM_INFO_IT_MAX (500 / LOOP_SLEEP_MS)
#define UPDATE_MOTOR_SPEED_IT_MAX (50 / LOOP_SLEEP_MS)
#define UPDATE_CAMERA_LIST_IT_MAX (2000 / LOOP_SLEEP_MS)

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
    UartHandler uartHandler;
    uartHandler.initUart();
    uint64_t update_misc_it = 0;
    uint64_t update_cam_info_it = 0;
    uint64_t update_camera_list_it = 0;
    uint64_t update_motor_speed_info_it = 0;
    std::vector<MessFromMQTT> mess_from_MQTT;
    std::vector<MessFromUART> mess_from_UART;
    ShmHandler shmHandler;
    Misc_Info misc_info;
    SHM_cameraInfo cam_info;
    SHM_cameraList camera_list;
    SHM_MotorControl motor_controls;
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
            else if (mqtt_message.topic == "guider/motor_speed")
            {
                nlohmann::json j = nlohmann::json::parse(mqtt_message.payload);
                int contains = 0;
                if (j.contains("ra_speed"))
                {
                    uartHandler.setRaSpeed(static_cast<float>(j["ra_speed"]));
                    contains++;
                }
                if (j.contains("dec_speed"))
                {
                    uartHandler.setDecSpeed(static_cast<float>(j["dec_speed"]));
                    contains++;
                }
                if (contains == 0)
                {
                    std::cerr << "Message does not have ra_speed or dec_speed" << std::endl;
                }
            }
            else if (mqtt_message.topic == "guider/select_camera_id")
            {
                int selected_id = -1;
                try
                {
                    nlohmann::json j = nlohmann::json::parse(mqtt_message.payload);
                    if (j.is_number_integer())
                    {
                        selected_id = static_cast<int>(j);
                    }
                    else if (j.contains("selectedCameraId"))
                    {
                        selected_id = static_cast<int>(j["selectedCameraId"]);
                    }
                    else if (j.contains("cameraId"))
                    {
                        selected_id = static_cast<int>(j["cameraId"]);
                    }
                }
                catch (const nlohmann::json::parse_error &e)
                {
                    try
                    {
                        selected_id = std::stoi(mqtt_message.payload);
                    }
                    catch (...)
                    {
                        selected_id = -1;
                    }
                }

                if (selected_id >= 0)
                {
                    if (shmHandler.selectCamera(selected_id) == 0)
                    {
                        std::cout << "Selected camera id set to " << selected_id << std::endl;
                    }
                    else
                    {
                        std::cerr << "Invalid selected camera id or camera unavailable: " << selected_id << std::endl;
                    }
                }
                else
                {
                    std::cerr << "Failed to parse selected camera id from payload: " << mqtt_message.payload << std::endl;
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

        if (update_cam_info_it % UPDATE_CAM_INFO_IT_MAX == 0)
        {
            shmHandler.readCameraInfo(cam_info);
            std::string message = "{ \"producer\": \"" + std::string(cam_info.procuder) + "\"," +
                                  "\"name \":\"" + std::string(cam_info.camera_name) + "\"," +
                                  "\"gain_range\": " + "[" + std::to_string(cam_info.gain_min) + "," + std::to_string(cam_info.gain_max) + "],"
                                                                                                                                           "\"exposure_range\": " +
                                  "[" + std::to_string(cam_info.exposure_min) + "," + std::to_string(cam_info.exposure_max) + "],";
            std::string data_types = "[";
            for (int i = 0; i < UNKNOWN_DATA_TYPE; i++)
            {
                if (cam_info.data_types[i] == true)
                {
                    data_types += std::to_string(i) + ",";
                }
            }
            if (data_types.back() == ',')
            {
                data_types.pop_back();
            }
            data_types = data_types + "]";
            message += "\"data_types\": " + data_types + ",";
            message += "\"mono\": " + std::to_string(cam_info.mono) + ",";
            message += "\"x_res\": " + std::to_string(cam_info.x_size) + ",";
            message += "\"y_res\": " + std::to_string(cam_info.y_size) + ",";
            message += "\"patt\": " + std::to_string(cam_info.patt);
            message += "}";
            mqtt_hander.publish("guider/camera_info", message);
        }

        if (update_camera_list_it % UPDATE_CAMERA_LIST_IT_MAX == 0)
        {
            shmHandler.readCameraList(camera_list);
            if (camera_list.ready)
            {
                nlohmann::json list_json;
                list_json["ready"] = camera_list.ready;
                list_json["selectedCameraId"] = camera_list.selectedCameraId;
                list_json["cameras"] = nlohmann::json::array();

                for (int i = 0; i < MAX_NUMER_OF_CAMERAS; ++i)
                {
                    nlohmann::json cam_json;
                    cam_json["available"] = camera_list.available[i];
                    cam_json["producer"] = std::string(camera_list.cameras[i].procuder);
                    cam_json["name"] = std::string(camera_list.cameras[i].camera_name);
                    cam_json["x_res"] = camera_list.cameras[i].x_size;
                    cam_json["y_res"] = camera_list.cameras[i].y_size;
                    cam_json["gain_range"] = {camera_list.cameras[i].gain_min, camera_list.cameras[i].gain_max};
                    cam_json["exposure_range"] = {camera_list.cameras[i].exposure_min, camera_list.cameras[i].exposure_max};
                    cam_json["mono"] = camera_list.cameras[i].mono;
                    cam_json["patt"] = camera_list.cameras[i].patt;
                    cam_json["data_types"] = nlohmann::json::array();
                    for (int type = 0; type < UNKNOWN_DATA_TYPE; ++type)
                    {
                        if (camera_list.cameras[i].data_types[type])
                        {
                            cam_json["data_types"].push_back(type);
                        }
                    }
                    list_json["cameras"].push_back(cam_json);
                }

                mqtt_hander.publish("guider/camera_list", list_json.dump());
            }
        }

        if (update_motor_speed_info_it % UPDATE_MOTOR_SPEED_IT_MAX == 0)
        {

            shmHandler.readMotorCtrlReq(motor_controls);
            if (motor_controls.updated)
            {
                uartHandler.setRaSpeed(motor_controls.ra_speed);
                uartHandler.setDecSpeed(motor_controls.dec_speed);
            }
        }
        update_misc_it += 1;
        update_cam_info_it += 1;
        update_camera_list_it += 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    // Destroy all semaphores and mutexes before exit
    destroyMessages(&mess_from_MQTT, &mess_from_UART);
    return 0;
}