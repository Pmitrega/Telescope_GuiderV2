#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H
#include <pthread.h>
#include <semaphore.h>
#include <vector>
#include "message_types.hpp"
#include "MQTTClient.h"

struct MqttMessage{
    std::string topic;
    std::string payload;
};


class MQTTHandler {
public:
    MQTTHandler(std::vector<MessFromMQTT>* from_mqtt_messages,
                std::vector<MessFromUART>* from_uart_messages);

    void publish(std::string topic, std::string payload); 
    int handleReceive(MqttMessage* mqtt_msg);
private:
    std::vector<MessFromMQTT>* m_from_mqtt_messages;
    std::vector<MessFromUART>* m_from_uart_messages;
    MQTTClient m_mqtt_client;
};


#endif // MQTT_HANDLER_H