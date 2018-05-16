
#include <wblib/testing/fake_mqtt.h>
#include <wblib/testing/fake_driver.h>
#include <wblib/driver_args.h>
#include <wblib/testing/testlog.h>
#include <stdio.h>
#include <gtest/gtest.h>
#include "onewire_driver.h"

using namespace std;
using namespace WBMQTT;
using namespace WBMQTT::Testing;

#define LOG(logger) ::logger.Log() << "[onewire driver test] "

class TOnewireDriverTest: public TLoggedFixture
{
protected:
    void SetUp();
    void TearDown();

    static const char * const Name;

    PFakeMqttBroker MqttBroker;
    PFakeMqttClient MqttClient;
    PDeviceDriver   Driver;
    unique_ptr<TOneWireDriver> driver;
};

const char * const TOnewireDriverTest::Name = "onewire-driver-test";

void TOnewireDriverTest::SetUp()
{
    SetMode(E_Normal);
    TLoggedFixture::SetUp();

    MqttBroker = NewFakeMqttBroker(*this);
    MqttClient = MqttBroker->MakeClient(Name);
    auto backend = NewDriverBackend(MqttClient);
    Driver = NewDriver(TDriverArgs{}
        .SetId(Name)
        .SetBackend(backend)
        .SetIsTesting(true)
        .SetReownUnknownDevices(true)
        .SetUseStorage(false)
    );

    Driver->StartLoop();
}


void TOnewireDriverTest::TearDown()
{
    Driver->StopLoop();
    TLoggedFixture::TearDown();
}

TEST_F(TOnewireDriverTest, create_and_read)
{
    TOneWireDriver w1_driver(Driver, 10, "./fake_sensors/1_sensor/");
    w1_driver.UpdateDevicesAndControls();
    w1_driver.UpdateSensorValues();
    Emit() << "Clear()";
    w1_driver.Clear();
}

TEST_F(TOnewireDriverTest, move_and_read)
{
    TOneWireDriver w1_driver(Driver, 10, "./fake_sensors/1_sensor/");
    w1_driver.UpdateDevicesAndControls();
    w1_driver.UpdateSensorValues();
    Emit() << "Moving sensor to tmp";
    rename("./fake_sensors/1_sensor/28-00000a013d97", "./fake_sensors/1_sensor/tmp-28-00000a013d97");
    w1_driver.UpdateDevicesAndControls();
    w1_driver.UpdateSensorValues();
    Emit() << "Moving sensor back";
    rename("./fake_sensors/1_sensor/tmp-28-00000a013d97", "./fake_sensors/1_sensor/28-00000a013d97");
    w1_driver.UpdateDevicesAndControls();
    w1_driver.UpdateSensorValues();
    Emit() << "Clear()";
    w1_driver.Clear();

}