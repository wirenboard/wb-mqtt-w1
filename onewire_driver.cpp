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


TOneWireDriver::TOneWireDriver (const WBMQTT::PDeviceDriver & mqttDriver) 
{
    TOneWireDriver (mqttDriver, DEFAULT_POLL_INTERVALL_MS);
}


TOneWireDriver::TOneWireDriver (const WBMQTT::PDeviceDriver & mqttDriver, int p_intvall_us) : MqttDriver(mqttDriver), poll_intervall_ms(p_intvall_us), Active(false)
{

    if (p_intvall_us <= 0) {
        throw invalid_argument("polling intervall must be greater than zero");
    }

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


    EventHandlerHandle = mqttDriver->On<TSyncEvent>([](const TSyncEvent & event){
        
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

                auto start_time = chrono::steady_clock::now();

                UpdateDevicesAndControls();      
                auto detected_devices = OneWireManager.GetDevicesP();
                if (detected_devices.empty()) {
                    LOG(Info) << "Device list is emtpy";
                    continue;
                }
                //read sensor data
                names.clear();
                values.clear();
                for (const auto sensor : detected_devices) {
                    LOG(Info) << "Read " << sensor.GetDeviceName();

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
                    LOG(Info) << "Reading list is emtpy";
                }

                // Timing
                auto finish_time = chrono::steady_clock::now();
                chrono::duration<double> elapsed = finish_time - start_time;
                this_thread::sleep_for(std::chrono::milliseconds(poll_intervall_ms -  (int)(elapsed.count() * 1000)));
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

void TOneWireDriver::UpdateDevicesAndControls()
{
    auto oldSensors = OneWireManager.GetDevicesP();
    OneWireManager.RescanBus();
    auto actualSensors = OneWireManager.GetDevicesP();

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