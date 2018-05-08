#pragma once
#include <wblib/wbmqtt.h>
#include <wblib/declarations.h>
#include <wblib/utils.h>
#include "sysfs_w1.h"
#include <iostream>
#include <cstdio>
#include <chrono>
#include <thread>
#include <cstring>
#include <fstream>
#include "exceptions.h"
#include "log.h"

#define DEFAULT_POLL_INTERVALL_MS       10000

enum class EOneWireDirection {
    Input,
};

class TOneWireDriver
{
    public:    
        static const char * const Name;

        TOneWireDriver(const WBMQTT::PDeviceDriver & mqttDriver);
        TOneWireDriver(const WBMQTT::PDeviceDriver & mqttDriver, int p_intvall_us);
        ~TOneWireDriver();

        void Start();
        void Stop();
        void Clear() noexcept;

    private:
        WBMQTT::PDeviceDriver              MqttDriver;
        WBMQTT::PDriverEventHandlerHandle  EventHandlerHandle;
        shared_ptr<WBMQTT::TLocalDevice>   DeviceP;

        TSysfsOnewireManager                OneWireManager;
        std::atomic_bool                    Active;
        std::unique_ptr<std::thread>        Worker;
        chrono::milliseconds                poll_intervall_ms;

        void RescanBus();
        void UpdateDevicesAndControls();
};