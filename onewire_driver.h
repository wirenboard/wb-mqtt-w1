#pragma once

#include "sysfs_w1.h"
#include "threaded_runner.h"

#include <wblib/log.h>
#include <wblib/wbmqtt.h>

class TOneWireDriverWorker : public IPeriodicalWorker
{
public:
    TOneWireDriverWorker(const std::string&           deviceId,
                         const WBMQTT::PDeviceDriver& mqttDriver,
                         WBMQTT::TLogger&             infoLogger,
                         WBMQTT::TLogger&             errorLogger,
                         const std::string&           thermometersSysfsDir);
    ~TOneWireDriverWorker();

    void RunIteration() override;

private:
    WBMQTT::PDeviceDriver MqttDriver;
    WBMQTT::PLocalDevice  Device;
    TSysfsOneWireManager  OneWireManager;
    WBMQTT::TLogger&      InfoLogger;
    WBMQTT::TLogger&      ErrorLogger;
    std::string           DeviceId;
    bool                  FirstTime;
};
