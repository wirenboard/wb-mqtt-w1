#include "onewire_driver.h"

#define LOG(logger) ::logger.Log() << "[w1 driver] "


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
    LOG(Debug) << "Finish rescan";

    try {

        auto tx = MqttDriver->BeginTx();

        DeviceP = tx->CreateDevice(TLocalDeviceArgs{}
            .SetId(Name)
            .SetTitle("1-wire Thermometers")
            .SetIsVirtual(true)
            .SetDoLoadPrevious(false)
        ).GetValue();

    } catch (const exception & e) {
        LOG(Error) << "Unable to create W1 driver: " << e.what();
        throw;
    }


    EventHandlerHandle = mqttDriver->On<TControlOnValueEvent>([](const TControlOnValueEvent & event){
        uint8_t value;
        if (event.RawValue == "1") {
            value = 1;
        } else if (event.RawValue == "0") {
            value = 0;
        } else {
            LOG(Warn) << "Invalid value: " << event.RawValue;
            return;
        }
    });
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

        vector<string> names;
        vector<double> values;

        while (Active.load()) {

                UpdateControls();        
                //read sensor data
                names.clear();
                values.clear();
                for (const auto sensor : OneWireManager.GetDevicesP()) {

                    auto res = sensor.ReadTemperature();
                    if (res.IsDefined()) {
                        names.push_back(sensor.GetDeviceName());
                        values.push_back(res.GetValue());
                    }
                }

                if (!names.empty()) {
                    auto tx     = MqttDriver->BeginTx();
                    auto device = tx->GetDevice(Name);

                    for (unsigned int i = 0; i < names.size(); i++) {
                    LOG(Info) << "Publish: " << names[i];
                        device->GetControl(names[i])->SetValue(tx, static_cast<double>(values[i]));
                    }

                } else {
                    LOG(Info) << "Device list is emtpy";
                }

            }
            LOG(Info) << "Stopped";


        }
    });

}

template<typename T>
void UnorderedVectorDifference(const vector<T> &first, const vector<T>& second, vector<T> & result)
{
    for (auto & el_first: first) {
        bool found = false;
        for (auto & el_second: second) {
            if (el_first == el_second) {
                found = true;
                break;
            }
        }

        if (!found) {
            result.push_back(el_first);
        }
    }
}

void TOneWireDriver::UpdateControls()
{
    LOG(Info) << "Start UpdateControls";

    auto oldSensors = OneWireManager.GetDevicesP();
    OneWireManager.RescanBus();
    auto actualSensors = OneWireManager.GetDevicesP();
    for (auto sensor : actualSensors) {
        LOG(Info) << "Scanned sensor: " << sensor.GetDeviceId() << sensor.GetDeviceName();
     }


    vector<TSysfsOnewireDevice> addSensors;
    UnorderedVectorDifference(actualSensors, oldSensors, addSensors);

    vector<TSysfsOnewireDevice> removeSensors;
    UnorderedVectorDifference(oldSensors, actualSensors, removeSensors);
   
    auto tx = MqttDriver->BeginTx();
    auto futureControl = TPromise<PControl>::GetValueFuture(nullptr);

    for (const auto sensor : addSensors) {
        LOG(Info) << "CreateControl for: " << sensor.GetDeviceId() << sensor.GetDeviceName();
        

        futureControl = DeviceP->CreateControl(tx, TControlArgs{}
            .SetId(sensor.GetDeviceName())
            .SetType("temperature")
            .SetReadonly( 0)
        );     
    }

    futureControl.Wait();   // wait for last control

    for (const auto sensor : removeSensors) {
        LOG(Info) << "RemoveControl for: " << sensor.GetDeviceId() << sensor.GetDeviceName();
        DeviceP->RemoveControl (tx, sensor.GetDeviceName());    
    }
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