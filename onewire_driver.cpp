#include "onewire_driver.h"

#define LOG(logger) ::logger.Log() << "[w1 driver] "


using namespace std;
using namespace WBMQTT;

const char * const TOneWireDriver::Name = "wb-w1";

namespace
{
    /*  @brief  function's exception thrown inside SuppressExceptions is not propagated only printed as a warning message
     *  @param  fn:     function pointer to execute
     *  @param  place:  additinal text to warning message
     */

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

    /*  @brief  Comparing two unordered vector and returns their difference
     *  @param  first:  base vector reference
     *  @param  second: secondary vector reference
     *  @param  result: result vector reference
     *  @note   first - second = result
     */

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
    
}

/*  @brief  class constructor, uses delegated constructor
 *  @param  mqttDriver: mqtt handler object, required for handling MQTT   
 *  @note   If functions is called, default polling interval is used
 */

TOneWireDriver::TOneWireDriver (const WBMQTT::PDeviceDriver & mqttDriver) : TOneWireDriver (mqttDriver, DEFAULT_POLL_INTERVALL_MS, string(SysfsOnewireDevicesPath))
{

}

/*  @brief  class constructor, it should be called at each object creation (other constructors should call it too)
 *  @param  mqttDriver:     mqtt handler object, required for handling MQTT   
 *  @param  p_intvall_ms:   polling intervall in microseconds
 *  @note   mqtt device is creater here however control channels are not
 */

TOneWireDriver::TOneWireDriver (const WBMQTT::PDeviceDriver & mqttDriver, int p_intvall_ms) : TOneWireDriver (mqttDriver, p_intvall_ms, string(SysfsOnewireDevicesPath))
{

}

/*  @brief  class constructor, it should be called at each object creation (other constructors should call it too)
 *  @param  mqttDriver:     mqtt handler object, required for handling MQTT   
 *  @param  p_intvall_ms:   polling intervall in microseconds
 *  @param  dir:            sensor directory
 *  @note   mqtt device is creater here however control channels are not
 */


TOneWireDriver::TOneWireDriver (const WBMQTT::PDeviceDriver & mqttDriver, int p_intvall_ms, const string& dir) : MqttDriver(mqttDriver), poll_intervall_ms(p_intvall_ms), Active(false), OneWireManager(dir)
{

    if (p_intvall_ms <= 0) {
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
}

/*  @brief class destructor function, clears all the MQTT devices and channels
 */

TOneWireDriver::~TOneWireDriver()
{
    if(!Cleaned.load()) {
        Clear();
    }
}

/*  @brief  Main operation function of  1W sensor driver. Scanning devices, updating mqtt channels, reading sensor values
 *  @note   The main loop is time controlled by "poll_intervall_ms". The available sensor devices are scanned and if there is any new,
 *          then a new MQTT channel is created, however any has disappeared, the channel is removed . 
 *          All the available sensor devices are read and their value is written to the corresponding mqtt channel. 
 *  @throws "TW1SensorDriverException" if the worker thread has been started already
 */

void TOneWireDriver::Start() 
{
    if (Active.load()) {
        wb_throw(TW1SensorDriverException, "attempt to start already started driver");
    }

    Active.store(true);
    Cleaned.store(false);
    Worker = WBMQTT::MakeThread("W1 worker", {[this]{
        LOG(Info) << "Started";

        vector<string> names;
        vector<double> values;

        while (Active.load()) {

                auto start_time = chrono::steady_clock::now();

                // update available sensor list, and mqtt control channels
                UpdateDevicesAndControls();      
                auto detected_devices = OneWireManager.GetDevicesP();
                if (detected_devices.empty()) {
                    LOG(Info) << "Device list is emtpy";
                    continue;
                }
                // read sensor data
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
                    // write sensor data to mqtt channels
                    auto tx     = MqttDriver->BeginTx();
                    auto device = tx->GetDevice(Name);

                    for (unsigned int i = 0; i < names.size(); i++) {
                        LOG(Info) << "Publish: " << names[i];
                        device->GetControl(names[i])->SetValue(tx, static_cast<double>(values[i]));
                    }

                } else {
                    LOG(Info) << "Reading list is emtpy";
                }

                // timing
                chrono::milliseconds elapsed;
                do {
                    this_thread::sleep_for(chrono::seconds(1));
                    elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start_time);

                } while (Active.load() && (elapsed < poll_intervall_ms ));
            }
            LOG(Info) << "Stopped";
        }
    });

}

/*  @brief  re-scanning available sensor devices and compare it with registered devices. If there is any new, it creates new channel, if any has 
 *          been removed, it removes channel
 */

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
        LOG(Info) << "CreateControl for: " << sensor.GetDeviceName();
        
        futureControl = DeviceP->CreateControl(tx, TControlArgs{}
            .SetId(sensor.GetDeviceName())
            .SetType("temperature")
            .SetReadonly(true)
        );     
    }

    futureControl.Wait();   // wait for last control

    for (const auto sensor : removeSensors) {
        LOG(Info) << "RemoveControl of: " << sensor.GetDeviceName();
        DeviceP->RemoveControl (tx, sensor.GetDeviceName());    
    }
}

/*  @brief  Stop "working" thread (Start function content). It will stop sensor reading and channel refreshing
 *  @throws "TW1SensorDriverException" if there is no such thread
 */

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

/*  @brief cleaning up all the generated channels
 */

void TOneWireDriver::Clear() noexcept
{
    if (Active.load()) {
        LOG(Error) << "Unable to clear driver while it's running";
        return;
    }

    LOG(Info) << "Cleaning...";

    SuppressExceptions([this]{
        MqttDriver->BeginTx()->RemoveDeviceById(Name).Sync();
    }, "TOneWireDriver::Clear()");

    SuppressExceptions([this]{
        OneWireManager.ClearDevices();
    }, "TOneWireDriver::Clear()");

    EventHandlerHandle = nullptr;

    Cleaned.store(true);
    LOG(Info) << "Cleaned";
}