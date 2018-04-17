#include "onewire_driver.h"

#define LOG(logger) ::logger.Log() << "[gpio driver] "


using namespace std;
using namespace WBMQTT;

const char * const TOneWireDriver::Name = "wb-w1";

namespace
{
    template <int N>
    inline bool EndsWith(const string & str, const char(& with)[N])
    {
        return str.rfind(with) == str.size() - (N - 1);
    }

    template <typename F>
    inline void SuppressExceptions(F && fn, const char * place)
    {
        try {
            fn();
        } catch (const exception & e) {
            LOG(Warn) << "Exception at " << place << ": " << e.what();
        } catch (...) {
            LOG(Warn) << "Unknown exception in " << place;
        }
    }
}

TOneWireDriver::TOneWireDriver (const WBMQTT::PDeviceDriver & mqttDriver) : MqttDriver(mqttDriver), Active(false)
{
    // scan bus to detect devices 
    LOG(Debug) << "Start rescan";
    OneWireManager.RescanBus();
    auto Sensors = OneWireManager.GetDevices();
    LOG(Debug) << "Finish rescan";

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
            LOG(Debug) << "CreateControl for: " << sensor.GetDeviceId() << sensor.GetDeviceName();

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


TOneWireDriver::~TOneWireDriver()
{
    if (EventHandlerHandle) {
        Clear();
    }
}


void TOneWireDriver::Start() 
{
    if (Active.load()) {
        wb_throw(TW1SensorDriverException, "attempt to start already started driver");
    }

    Active.store(true);

    Worker = WBMQTT::MakeThread("W1 worker", {[this]{
        LOG(Info) << "Started";

        while (Active.load()) {
        
            auto tx     = MqttDriver->BeginTx();
            auto device = tx->GetDevice(Name);
            for (const auto sensor : OneWireManager.GetDevices()) {
                device->GetControl(sensor.GetDeviceName())->SetValue(tx, static_cast<double>(55.66));
            }
        }
        LOG(Info) << "Stopped";


    }});

}


void TOneWireDriver::Stop()
{
    if (!Active.load()) {
        wb_throw(TW1SensorDriverException, "attempt to stop not started driver");
    }

    Active.store(false);
    LOG(Info) << "Stopping...";

    if (Worker->joinable()) {
        Worker->join();
    }

    Worker.reset();
}

void TOneWireDriver::Clear() noexcept
{
    if (Active.load()) {
        LOG(Error) << "Unable to clear driver while it's running";
        return;
    }

    LOG(Info) << "Cleaning...";

    SuppressExceptions([this]{
        MqttDriver->RemoveEventHandler(EventHandlerHandle);
    }, "TOneWireDriver::Clear()");

    SuppressExceptions([this]{
        MqttDriver->BeginTx()->RemoveDeviceById(Name).Sync();
    }, "TOneWireDriver::Clear()");

    SuppressExceptions([this]{
        OneWireManager.ClearDevices();
    }, "TOneWireDriver::Clear()");

    EventHandlerHandle = nullptr;

    LOG(Info) << "Cleaned";
}