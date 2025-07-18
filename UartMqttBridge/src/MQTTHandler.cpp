#include "MQTTHandler.hpp"
#include "message_types.hpp"
#include "MQTTClient.h"
#include <queue>
#include <iostream>

#define ADDRESS     "tcp://localhost:1883"
#define CLIENTID    "UartMqttBridge"
#define QOS         0
#define TIMEOUT     10000L



static std::queue<MqttMessage> mqtt_message_queue;


void delivered(void *context, MQTTClient_deliveryToken dt)
{
    printf("Message with token value %d delivery confirmed\n", dt);
    // deliveredtoken = dt;
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    std::string topic(topicName);
    char payload_buffer[100];
    sprintf(payload_buffer,"%.*s", message->payloadlen, (char*)message->payload);
    std::string payload(payload_buffer);
    std::cout << "topic: " << topic <<"payload: " << payload << std::endl;
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    mqtt_message_queue.push({topic, payload});
    return 1;
}

void connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    if (cause)
    	printf("     cause: %s\n", cause);
}


MQTTHandler::MQTTHandler(std::vector<MessFromMQTT>* from_mqtt_messages,
                         std::vector<MessFromUART>* from_uart_messages)
    : m_from_mqtt_messages(from_mqtt_messages),
      m_from_uart_messages(from_uart_messages)
{

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_create(&m_mqtt_client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    int rc;
    if((rc= MQTTClient_setCallbacks(m_mqtt_client, NULL, connlost, msgarrvd , delivered))!= MQTTCLIENT_SUCCESS)
    {
        printf("Failed to set callbacks, return code %d\n", rc);
        rc = EXIT_FAILURE;
        MQTTClient_destroy(&m_mqtt_client);
    }

    rc = MQTTClient_connect(m_mqtt_client, &conn_opts);
    if( rc != MQTTCLIENT_SUCCESS){
        std::cerr << "Falied to connect to mqtt"<<std::endl;
    }
    MQTTClient_subscribe(m_mqtt_client, "guider/camera_controls", 0);
    std::cout << "Subscribed for guider/camera_controls" << std::endl;
}
void MQTTHandler::handleTransmission(){

}

int MQTTHandler::handleReceive(MqttMessage* mqtt_msg){
  MqttMessage mqtt_message;
  if(!mqtt_message_queue.empty()){
        mqtt_message = mqtt_message_queue.front();
        mqtt_message_queue.pop();
        mqtt_msg->payload = mqtt_message.payload;
        mqtt_msg->topic = mqtt_message.topic;
        return 0;
    }
    return 1;
}