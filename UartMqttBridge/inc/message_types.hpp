#ifndef MESSAGETYPES_H
#define MESSAGETYPES_H
#include <string>
#include <semaphore.h>
#include <pthread.h>
#include <tuple>



namespace MESSAGE_TYPE{
    enum FMQTT_MESSAGE_TYPE{
        SET_RA_SPEED = 0,
        SET_DEC_SPEED,
        REQ_BATT_VOLTAGE,
        REQ_BATT_CURRENT,
        FMQTT_MESSAGE_SIZE
    };

    enum FUART_MESSAGE_TYPE{
        POST_RA_SPEED = 0,
        POST_DEC_SPEED,
        POST_BATT_VOLTAGE,
        POST_BATT_CURRENT,
        FUART_MESSAGE_SIZE
    };

    struct message{
        std::string str_mess;
        float f_message;
        int i_message;
    };
}

using MessFromMQTT = std::tuple<sem_t, pthread_mutex_t, MESSAGE_TYPE::FMQTT_MESSAGE_TYPE, MESSAGE_TYPE::message>;
using MessFromUART = std::tuple<sem_t, pthread_mutex_t, MESSAGE_TYPE::FUART_MESSAGE_TYPE, MESSAGE_TYPE::message>;

#endif