#include <wblib/wbmqtt.h>
#include <wblib/declarations.h>
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
        TSysfsOnewireManager                OneWireManager;
        std::atomic_bool                    Active;
        WBMQTT::PDeviceDriver               MqttDriver;
        WBMQTT::PDriverEventHandlerHandle   EventHandlerHandle;

        void RescanBus();

};