
#include "mqtt.h"
#include <iostream>
#include <chrono>
#include <thread>

#include <unistd.h>

using namespace std::chrono_literals;

std::atomic<uint32_t> MQTT::s_RefCnt(0);

bool MQTT::publish( std::string topic, const char* payload, size_t payloadlen, int qos ){
    int mid;
    return mosquitto_publish( m_Mosq, &mid, topic.c_str(), payloadlen, payload, qos, false ) == MOSQ_ERR_SUCCESS;
}

void MQTT::mqtt_msg_cb( struct mosquitto* mqtt, void* mqtt_new_data, const struct mosquitto_message* omsg )
{
    std::string topic = std::string(omsg->topic);
    if( omsg->payloadlen == 0 ) {
        std::cout << "got message on topic " << topic << std::endl;
        std::cout << "with no content" << std::endl;
        return;
    }
    std::string msg = std::string( (const char*)omsg->payload, omsg->payloadlen);

    MQTT *myself = (MQTT *)mqtt_new_data;

    for( auto item: myself->m_TopicList)
    {
        bool match;
        mosquitto_topic_matches_sub( item.topic.c_str(),
                                    topic.c_str(),
                                    &match);
        if( match ) item.handler( (uint8_t*)omsg->payload, omsg->payloadlen );
    }
}


MQTT::MQTT(Json::Value config)
{
    if( s_RefCnt + 1 == 1 ){
        s_RefCnt++;
        mosquitto_lib_init();
    }

    m_Mosq = mosquitto_new( nullptr, true, this );

    auto host = config["server"].asString();
    auto port = config["port"].asInt();
    auto keepalive = config["keepalive"].asInt();
    if( mosquitto_connect( m_Mosq, host.c_str(), port, keepalive ) < 0 )
    {
        std::cerr << "failed to connect to mqtt" << std::endl;
    }

    mosquitto_message_callback_set( m_Mosq, mqtt_msg_cb );
}

MQTT::~MQTT()
{
    if( s_RefCnt - 1 > 0 ) {
        s_RefCnt--;
        mosquitto_lib_cleanup();
    }
}

bool MQTT::add_callback( std::string topic, std::function<void(uint8_t*,size_t)> handler )
{
    if( mosquitto_subscribe( m_Mosq, NULL, topic.c_str(), 0 ) < 0 )
    {
        std::cerr << "failed to subscribe mqtt to " << topic << std::endl;
        return false;
    }

    m_TopicList.push_back( mqtt_topic(topic,handler) );
    return true;
}

int MQTT::loop()
{
    int result = mosquitto_loop( m_Mosq, 1000, 1 );
    switch (result)
    {
        case 0:
            // No Error at all
            break;
        case MOSQ_ERR_NO_CONN:
        case MOSQ_ERR_CONN_LOST:
            std::this_thread::sleep_for( 1s );
            result = mosquitto_reconnect( m_Mosq );
            std::cerr << "Connection lost, try to reconnect: " << result << std::endl;
            break;
        default:
            std::cerr << "Error during mqtt operation: " << result << std::endl;
            break;
    }
    return result;
}
