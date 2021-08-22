#pragma once

#include <mutex>
#include <cstdint>
#include <map>
#include <list>
#include <functional>
#include <filesystem>
#include <thread>

class EvDevice {
    public:
        EvDevice( uint16_t, uint16_t );
        virtual ~EvDevice();

        bool add_callback(std::function<void(uint16_t, uint16_t)>);

    private:
        // member functions
        std::pair<bool, std::string> open();
        bool device_match();

        std::pair<bool, std::pair<uint16_t, uint16_t>> get_events(uint16_t type);
        void worker_thread(void);

        // members
        const uint16_t m_Vendor;
        const uint16_t m_Product;
        int32_t m_Fd;

        bool m_KeepRunning;

        std::thread m_Worker;


        std::mutex m_Mtx;
        std::map<int, std::list<std::function<void(uint16_t value)>>> m_Callbacks;

        static const std::string DEV_INPUT_EVENT;

};
