#include <cerrno>
#include <exception>
#include <unistd.h>
#include <signal.h>
#include <iostream>
#include <fstream>

#include <fcntl.h>
#include <inttypes.h>
#include <sys/types.h>
#include <stdlib.h>

#include <sys/types.h>
#include <err.h>

#include "mqtt.h"

#include <json/json.h>

#include "evdev.h"
#include <thread>
#include <sstream>
#include <iomanip>
#include <ctime>

#include <iomanip>
#include <chrono>

static void __attribute__((noreturn))
usage(void)
{
#if defined(__FreeBSD__)
    printf("usage: %s [-d evdev-device] [-c config-file]\n", getprogname());
#elif defined(__linux__)
    printf("usage: %s [-d evdev-device] [-c config-file]\n", program_invocation_name );
#endif
    exit(1);
}

static std::map<std::string, float> counters;

static float round2( float value )
{
    return (int)(value * 100.0 + 0.5) / 100.0;
}

void update_value( std::string& message )
{
    JSONCPP_STRING err;
    Json::Value msgval;
    Json::CharReaderBuilder builder;
    std::istringstream in( message );
    printf("Got message: %s\n", message.c_str() );

    auto parsingOk = parseFromStream( builder, in, &msgval, &err );
    if( parsingOk ){
        try {
            if( msgval.isMember("name") && msgval.isMember("value") )
            {
                auto name = msgval["name"].asString();
                auto value = msgval["value"].asFloat();
                counters[name] = value;
                printf("updated %s to %1.2f\n", name.c_str(), value );
            }
        }catch( Json::Exception e ) {
            printf("catched Exception: %s\n", e.what() );
        }
    } else printf("parsing not ok\n");

}

int main( int argc, char* argv[] )
{
    JSONCPP_STRING errs;
    Json::Value root;

    std::string config_filename = "/usr/local/etc/doorbell/config.json";

    signed char ch;
    while ((ch = getopt(argc, argv, "c:h")) != -1) {
        switch (ch) {
        case 'c':
            config_filename = std::string(optarg);
            break;
        case 'h':
            usage();
        default:
            break;
        }
    }
    argc -= optind;
    argv += optind;

    std::ifstream config( config_filename, std::ifstream::binary );

    Json::CharReaderBuilder builder;
    if (!parseFromStream( builder, config, &root, &errs )) {
        std::cout << errs << std::endl;
        return EXIT_FAILURE;
    }

    MQTT mqtt(root["mqtt-configuration"]);
    auto base_topic = root["base_topic"].asString();

    mqtt.add_callback( base_topic+"/stats", [](uint8_t*msg, size_t len)
            {
                std::string strmsg( (char*)msg, len );
                update_value( strmsg );

            } );

    uint16_t vendor_number = std::stol(root["input"]["vendor"].asString(), nullptr, 0);
    uint16_t product_number = std::stol(root["input"]["product"].asString(), nullptr, 0);

    EvDevice evdev( vendor_number, product_number );

    for( auto &sensor : root["sensors"] )
    {
        try { 
            auto evdev_code = sensor["event"].asUInt();
            auto impulse = sensor["impulse"].asFloat();
            auto name = sensor["name"].asString();
            auto unit = sensor["unit"].asString();

            std::cout << "------------------------" << std::endl;
            std::cout << "CODE:    " << evdev_code << std::endl;
            std::cout << "NAME:    " << name << std::endl;
            std::cout << "UNIT:    " << unit << std::endl;
            std::cout << "IMPULSE: " << impulse << std::endl;
            std::cout << "------------------------" << std::endl;

            evdev.add_callback( evdev_code, [&mqtt, &base_topic, name, unit, impulse](uint16_t code)
                    {
                        if( code == 0 ) return;

                        Json::Value info;
                        float& counter = counters[name];

                        info[unit] = counter;

                        Json::StreamWriterBuilder wr;
                        wr.settings_["precision"] = 5;

                        counter = round2( counter + impulse );

                        std::string msg = Json::writeString(wr, info);
                        mqtt.publish( base_topic+"/"+name+"/amount", msg.c_str(), msg.length(), 0 );
                    });

        } catch(std::exception &e) {
            std::cerr << "Exception happened: " << e.what() << std::endl;

            exit( 1 );
        }
    }

    printf("Starting mqtt-loop\n");
    while(1) mqtt.loop();

    printf("Exit program\n");

    return 0;
}
