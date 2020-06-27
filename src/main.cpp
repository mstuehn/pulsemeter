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

#include <dirent.h>

#include "mqtt.h"

#include <json/json.h>

#include "evdev.h"
#include <thread>
#include <sstream>
#include <iomanip>
#include <ctime>

#include <iomanip>
#include <chrono>

static std::string now() {
    auto t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::stringstream wss;
    wss << std::put_time(&tm, "%H:%M:%S %d-%m-%Y");
    return wss.str();
}

static void __attribute__((noreturn))
usage(void)
{
    printf("usage: %s [-d evdev-device] [-c config-file]\n", getprogname());
    exit(1);
}

static float water_m3 = 0.0;
static float gas_m3 = 0.0;

int main( int argc, char* argv[] )
{
    Json::Value root;
    Json::Reader rd;
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
    bool parsingOk = rd.parse(config, root, false);

    if( !parsingOk )
    {
        std::cerr << "Error during reading " << config_filename << std::endl;
        exit( 2 );
    }

    MQTT mqtt(root["mqtt-configuration"]);
    auto base_topic = root["base_topic"].asString();

    mqtt.add_callback( base_topic+"/water/cmd/m3", [](uint8_t*msg, size_t len)
            {
                Json::Value msgval;
                Json::Reader rd;
                std::string strmsg( (char*)msg, len );
                printf("Got message: %s\n", strmsg.c_str() );
                std::istringstream in(strmsg);
                bool parsingOk = rd.parse(in , msgval, false);

                if( parsingOk ){
                    try {
                        water_m3 = msgval["set"].asFloat();
                    }catch( Json::Exception e ) {
                        printf("catched Exception: %s\n", e.what() );
                    }
                }

            } );

    mqtt.add_callback( base_topic+"/gas/cmd/m3", [](uint8_t*msg, size_t len)
            {
                Json::Value msgval;
                Json::Reader rd;
                std::string strmsg( (char*)msg, len );
                printf("Got message: %s\n", strmsg.c_str() );
                std::istringstream in(strmsg);
                bool parsingOk = rd.parse(in , msgval, false);

                if( parsingOk ){
                    try {
                        gas_m3 = msgval["set"].asFloat();
                    }catch( Json::Exception e ) {
                        printf("catched Exception: %s\n", e.what() );
                    }
                }

            } );

    std::thread evdevpoll([ &mqtt , &base_topic, &root ](){
            uint32_t vendor_number;
            sscanf(root["input"]["vendor"].asString().c_str(), "%x", &vendor_number);

            uint32_t product_number;
            sscanf(root["input"]["product"].asString().c_str(), "%x", &product_number);

            char* filename;
            do {
                filename = scan_devices(vendor_number, product_number);
            } while( filename == NULL );

            std::cout << "Opening " << filename << std::endl;

            int fd;
            if( (fd = open(filename, O_RDONLY) ) < 0) {
                perror("");
                if (errno == EACCES && getuid() != 0) {
                    fprintf(stderr, "You do not have access to %s. Try "
                            "running as root instead.\n", filename);
                    }
                    return;
            }

            while( 1 ) {
                uint16_t code, value;
                if( get_events( fd, EV_KEY, &code, &value ) && value == 1 )
                {
                    Json::StreamWriterBuilder wr;
                    wr.settings_["precision"] = 3;

                    switch( code ) {
                        case KEY_F21:
                        {
                            gas_m3 += 0.01;

                            Json::Value info;
                            info["m3"] = gas_m3;
                            std::string msg = Json::writeString(wr, info);
                            mqtt.publish(base_topic+"/gas/values", msg.c_str(), msg.length(), 0 );
                            std::cout << "Water: " << water_m3 << std::endl;
                        }break;
                        case KEY_F22:
                        {
                            water_m3 += 0.1;

                            Json::Value info;
                            info["m3"] = water_m3;
                            std::string msg = Json::writeString(wr, info);
                            mqtt.publish(base_topic+"/water/values", msg.c_str(), msg.length(), 0 );
                            std::cout << "Gas: " << water_m3 << std::endl;
                        } break;
                    }
            }
        }
    });

    printf("Starting mqtt-loop\n");
    while(1) mqtt.loop();

    printf("Exit program\n");

    evdevpoll.join();
    return 0;
}
