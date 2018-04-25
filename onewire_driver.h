#pragma once
#include <wblib/wbmqtt.h>
#include <wblib/declarations.h>
#include <wblib/utils.h>
#include "sysfs_w1.h"
#include <iostream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include "exceptions.h"
#include "log.h"

enum class EOneWireDirection {
    Input,
};

class TOneWireDriver
{
    public:    
        static const char * const Name;

        TOneWireDriver(const WBMQTT::PDeviceDriver & mqttDriver);
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
        void RescanBus();
        void UpdateDevicesAndControls();
};