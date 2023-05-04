#include "onewire_driver.h"
#include <functional>

#include <algorithm>

#define LOG(logger) logger.Log() << "[w1 driver] "

using namespace std;
using namespace WBMQTT;

namespace
{
    void DeleteControl(const TSysfsOneWireThermometer& sensor, PLocalDevice device, PDriverTx& tx, TLogger& infoLogger)
    {
        LOG(infoLogger) << "RemoveControl of: " << sensor.GetId();
        device->RemoveControl(tx, sensor.GetId()).Sync();
    }

    void UpdateValue(const TSysfsOneWireThermometer& sensor, PLocalDevice device, PDriverTx& tx, TLogger& errorLogger)
    {
        try {
            device->GetControl(sensor.GetId())->SetValue(tx, sensor.GetTemperature()).Sync();
        } catch (const exception& er) {
            LOG(errorLogger) << er.what();
            device->GetControl(sensor.GetId())->SetError(tx, "r").Sync();
        }
    }

    void CreateControl(const TSysfsOneWireThermometer& sensor, PLocalDevice device, PDriverTx& tx, TLogger& errorLogger)
    {
        try {
            device
                ->CreateControl(tx,
                                TControlArgs{}
                                    .SetId(sensor.GetId())
                                    .SetType("temperature")
                                    .SetReadonly(true)
                                    .SetRawValue(FormatFloat(sensor.GetTemperature())))
                .GetValue();
        } catch (const exception& er) {
            LOG(errorLogger) << er.what();
            device
                ->CreateControl(
                    tx,
                    TControlArgs{}.SetId(sensor.GetId()).SetType("temperature").SetReadonly(true).SetError("r"))
                .GetValue();
        }
    }
} // namespace

TOneWireDriverWorker::TOneWireDriverWorker(const string& deviceId,
                                           const PDeviceDriver& mqttDriver,
                                           TLogger& infoLogger,
                                           TLogger& debugLogger,
                                           TLogger& errorLogger,
                                           const string& thermometersSysfsDir)
    : MqttDriver(mqttDriver),
      OneWireManager(thermometersSysfsDir, debugLogger, errorLogger),
      InfoLogger(infoLogger),
      DebugLogger(debugLogger),
      ErrorLogger(errorLogger),
      DeviceId(deviceId),
      FirstTime(true)
{
    auto tx = MqttDriver->BeginTx();
    Device = tx->CreateDevice(TLocalDeviceArgs{}
                                  .SetId(DeviceId)
                                  .SetTitle("1-wire Thermometers")
                                  .SetIsVirtual(true)
                                  .SetDoLoadPrevious(false))
                 .GetValue();
}

void TOneWireDriverWorker::RunIteration()
{
    LOG(DebugLogger) << "Rescan bus";
    auto devices = OneWireManager.RescanBusAndRead();
    auto tx = MqttDriver->BeginTx();

    for (auto sensor: devices) {
        switch (sensor->GetStatus()) {
            case TSysfsOneWireThermometer::New:
                CreateControl(*sensor, Device, tx, ErrorLogger);
                break;
            case TSysfsOneWireThermometer::Connected:
                UpdateValue(*sensor, Device, tx, ErrorLogger);
                break;
            case TSysfsOneWireThermometer::Disconnected:
                DeleteControl(*sensor, Device, tx, InfoLogger);
                break;
        }
    }

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