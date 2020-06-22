#include <atomic>
#include <string>
#include <functional>
#include <list>

#include <mosquitto.h>

#include <json/json.h>

class MQTT {
    public:
        MQTT(Json::Value config);
        virtual ~MQTT();

        bool add_callback( std::string topic, std::function<void(uint8_t*,size_t)> );

        bool publish( std::string topic, const char* payload, size_t payloadlen, int qos);

        int loop();

    private:
        MQTT() = delete;

        struct mosquitto* m_Mosq;

        static std::atomic<uint32_t> s_RefCnt;

        struct mqtt_topic {
            mqtt_topic( std::string topic, std::function<void(uint8_t*, size_t)> handler ) :
                topic(topic), handler(handler) {};
            const std::string topic;
            std::function<void(uint8_t* msg, size_t len)> handler;
        };

        static void mqtt_msg_cb( struct mosquitto* mqtt, void* mqtt_new_data, const struct mosquitto_message* omsg );

        std::list<mqtt_topic> m_TopicList;
};

