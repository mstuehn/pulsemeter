#include "evdev.h"

#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>


const std::string EvDevice::DEV_INPUT_EVENT = { "/dev/input" };

static void print_driver_version( int fd )
{
    int version;
    if( ioctl(fd, EVIOCGVERSION, &version) ) {
        perror("can't get version");
    } else {
        printf("Input driver version is %d.%d.%d\n",
                version >> 16, (version >> 8) & 0xff, version & 0xff);
    }

}

bool EvDevice::device_match(){
    uint16_t id[4];
    if( ioctl(m_Fd, EVIOCGID, id) ) return false;

    printf("Input device ID: bus 0x%x vendor 0x%x product 0x%x version 0x%x\n\n",
            id[ID_BUS], id[ID_VENDOR], id[ID_PRODUCT], id[ID_VERSION]);

    return (id[ID_VENDOR] == m_Vendor ) && (id[ID_PRODUCT] == m_Product );
}

std::pair<bool, std::pair<uint16_t, uint16_t>> EvDevice::get_events(uint16_t type)
{
    struct input_event ev;
    auto size = ::read(m_Fd, &ev, sizeof(struct input_event));

    if( size < (ssize_t)sizeof(struct input_event) ) {
        printf("expected %lu bytes, got %li\n", sizeof(struct input_event), size);
        perror("\nerror reading");
        return { false, {0,0}};
    }

    if( type != ev.type ) return { false, {0,0}};

    return { true, {ev.code, ev.value }};
}


std::pair<bool, std::string> EvDevice::open()
{
    printf("Scanning for compatible Vendor/Product:  0x%04X/0x%04X\n", m_Vendor, m_Product);
    printf("Available devices:\n");

    std::string filename;
    for( auto const& dir: std::filesystem::directory_iterator{DEV_INPUT_EVENT} ) {
        char name[256] = "???";
        const std::string& fname = dir.path();

        int m_Fd = ::open(fname.c_str(), O_RDONLY);
        if( m_Fd < 0 ) continue;

        int result = ioctl( m_Fd, EVIOCGNAME(sizeof(name)), name );
        if( result < 0 ) { close(m_Fd); continue; }
        printf( "%s:  %s\n", fname.c_str(), name );

        if( !device_match()) { close(m_Fd); continue; }

        printf( "Device found: %s \n", fname.c_str() );
        return { true, "Device found: " + fname };
    }

    return { false, "No devices match configured vendor/product" } ;
}


EvDevice::EvDevice( uint16_t vendor, uint16_t product ) :
    m_Vendor( vendor ), m_Product( product ), m_KeepRunning(true),
    m_Worker([this](){
        while( m_KeepRunning ) {
            auto result = open();
            if( result.first ) {
                std::cout << "Device found, begin monitoring" << std::endl;
                break;
            }
            else {
                std::cout << "Scanning devices not successfull: " << std::endl;
                std::cout << ">>" << result.second << "<<" << std::endl;
                std::cout << "Rescan after 5 seconds" << std::endl;
                sleep(5);
            }
        };

        while( m_KeepRunning ) {
            auto result = get_events( EV_KEY );
            const uint16_t code = result.second.first;
            const uint16_t value = result.second.second;

            if( result.first )
            {
                if( !m_Callbacks.count(code) && m_Callbacks[code].empty() ) continue;

                for( auto& callback : m_Callbacks[code] ) {
                    callback( value );
                }
            }
        }
    })
{

}

EvDevice::~EvDevice()
{
    m_KeepRunning = false;
    m_Worker.join();
}

void EvDevice::worker_thread(void)
{
}

