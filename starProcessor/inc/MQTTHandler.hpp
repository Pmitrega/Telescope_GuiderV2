#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H
#include <pthread.h>
#include <semaphore.h>
#include <vector>
#include <string>
#include "MQTTClient.h"

struct MqttMessage{
    std::string topic;
    std::string payload;
};


class MQTTHandler {
public:
    MQTTHandler();
    void publish(std::string topic, std::string payload); 
    int handleReceive(MqttMessage* mqtt_msg);
private:
    MQTTClient m_mqtt_client;
};


#endif // MQTT_HANDLER_H