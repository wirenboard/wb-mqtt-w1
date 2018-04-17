#include "onewire_driver.h"

#define LOG(logger) ::logger.Log() << "[gpio driver] "


using namespace std;
using namespace WBMQTT;

const char * const TOneWireDriver::Name = "wb-w1";


TOneWireDriver::TOneWireDriver (const WBMQTT::PDeviceDriver & mqttDriver) : MqttDriver(mqttDriver), Active(false)
{
    // scan bus to detect devices 
    OneWireManager.RescanBus();
    auto Sensors = OneWireManager.GetDevices();
    try {

        auto tx = MqttDriver->BeginTx();
        auto device = tx->CreateDevice(TLocalDeviceArgs{}
            .SetId(Name)
            .SetTitle("test_title")
            .SetIsVirtual(true)
            .SetDoLoadPrevious(false)
        ).GetValue();

        auto futureControl = TPromise<PControl>::GetValueFuture(nullptr);

        for (const auto & sensor : Sensors) {

            futureControl = device->CreateControl(tx, TControlArgs{}
                .SetId(sensor.GetDeviceName())
                .SetType("temperature") //?
                .SetReadonly( 0)
            );
        }

        futureControl.Wait();   // wait for last control

        if (Sensors.empty()) {
            wb_throw(TW1SensorDriverException, "Failed to create any chip driver. Nothing to do");
        }

    } catch (const exception & e) {
        LOG(Error) << "Unable to create GPIO driver: " << e.what();
        throw;
    }
}
