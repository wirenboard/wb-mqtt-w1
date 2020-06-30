#include "onewire_driver.h"
#include <functional>

#include <algorithm>

#define LOG(logger) logger.Log() << "[w1 driver] "

using namespace std;
using namespace WBMQTT;

namespace
{
    template <class T> struct Partition
    {
        vector<T> OnlyFirst;
        vector<T> Same;
        vector<T> OnlySecond;
    };

    template <class T> bool contains(const T& c, const typename T::value_type& v)
    {
        return any_of(c.begin(), c.end(), [&](const auto& cv) { return cv == v; });
    }

    template <class T> Partition<T> FindRelation(const vector<T>& first, const vector<T>& second)
    {
        Partition<T> res;
        partition_copy(first.begin(),
                       first.end(),
                       back_inserter(res.Same),
                       back_inserter(res.OnlyFirst),
                       [&](const auto& v) { return contains(second, v); });
        copy_if(second.begin(), second.end(), back_inserter(res.OnlySecond), [&](const auto& v) {
            return !contains(res.Same, v);
        });
        return res;
    }

    void DeleteControls(const vector<TSysfsOneWireThermometer>& c,
                        PLocalDevice                            device,
                        PDriverTx&                              tx,
                        TLogger&                                infoLogger)
    {
        for (const auto sensor : c) {
            LOG(infoLogger) << "RemoveControl of: " << sensor.GetId();
            device->RemoveControl(tx, sensor.GetId()).Wait();
        }
    }

    void UpdateValues(const vector<TSysfsOneWireThermometer>& c,
                      PLocalDevice                            device,
                      PDriverTx&                              tx,
                      TLogger&                                errorLogger)
    {
        for (const auto sensor : c) {
            try {
                device->GetControl(sensor.GetId())->SetValue(tx, sensor.ReadTemperature()).Wait();
            } catch (const exception& er) {
                LOG(errorLogger) << er.what();
                device->GetControl(sensor.GetId())->SetError(tx, "r").Wait();
            }
        }
    }

    void CreateControls(const vector<TSysfsOneWireThermometer>& c,
                        PLocalDevice                            device,
                        PDriverTx&                              tx,
                        TLogger&                                errorLogger)
    {
        for (const auto sensor : c) {
            try {
                device->CreateControl(tx,
                                      TControlArgs{}
                                          .SetId(sensor.GetId())
                                          .SetType("temperature")
                                          .SetReadonly(true)
                                          .SetRawValue(FormatFloat(sensor.ReadTemperature()))).Wait();
            } catch (const exception& er) {
                LOG(errorLogger) << er.what();
                device->CreateControl(tx,
                                      TControlArgs{}
                                          .SetId(sensor.GetId())
                                          .SetType("temperature")
                                          .SetReadonly(true)
                                          .SetError("r")).Wait();
            }
        }
    }
} // namespace

TOneWireDriverWorker::TOneWireDriverWorker(const string&        deviceId,
                                           const PDeviceDriver& mqttDriver,
                                           TLogger&             infoLogger,
                                           TLogger&             errorLogger,
                                           const string&        thermometersSysfsDir)
    : MqttDriver(mqttDriver), OneWireManager(thermometersSysfsDir), InfoLogger(infoLogger), ErrorLogger(errorLogger),
      DeviceId(deviceId), FirstTime(true)
{
    auto tx = MqttDriver->BeginTx();
    Device  = tx->CreateDevice(TLocalDeviceArgs{}
                                  .SetId(DeviceId)
                                  .SetTitle("1-wire Thermometers")
                                  .SetIsVirtual(true)
                                  .SetDoLoadPrevious(false))
                 .GetValue();
}

void TOneWireDriverWorker::RunIteration()
{
    auto oldSensors = OneWireManager.GetDevices();

    try {
        OneWireManager.RescanBus();
    } catch (const std::exception& e) {
        LOG(ErrorLogger) << "Can't rescan bus: " << e.what();
        auto tx = MqttDriver->BeginTx();
        for (const auto sensor : oldSensors) {
            Device->GetControl(sensor.GetId())->SetError(tx, "r");
        }
        return;
    }

    auto part = FindRelation(oldSensors, OneWireManager.GetDevices());
    auto tx   = MqttDriver->BeginTx();

    DeleteControls(part.OnlyFirst, Device, tx, InfoLogger);
    UpdateValues(part.Same, Device, tx, ErrorLogger);
    CreateControls(part.OnlySecond, Device, tx, ErrorLogger);
    if (FirstTime) {
        Device->RemoveUnusedControls(tx).Wait();
        FirstTime = false;
    }
}

TOneWireDriverWorker::~TOneWireDriverWorker()
{
    try {
        MqttDriver->BeginTx()->RemoveDeviceById(DeviceId).Sync();
    } catch (const std::exception& e) {
        LOG(ErrorLogger) << "Exception during ~TOneWireDriverWorker: " << e.what();
    } catch (...) {
        LOG(ErrorLogger) << "Unknown exception during ~TOneWireDriverWorker";
    }
}