
#include "onewire_driver.h"
#include <fstream>
#include <gtest/gtest.h>
#include <stdio.h>
#include <wblib/driver_args.h>
#include <wblib/testing/fake_driver.h>
#include <wblib/testing/fake_mqtt.h>
#include <wblib/testing/testlog.h>

using namespace std;
using namespace WBMQTT;
using namespace WBMQTT::Testing;

const string DeviceId("wb-w1");

class TOnewireDriverTest: public TLoggedFixture
{
protected:
    string test_sensor_dir;

    void SetUp()
    {
        SetMode(E_Unordered);
        TLoggedFixture::SetUp();

        char* d = getenv("TEST_DIR_ABS");
        if (d != NULL) {
            test_sensor_dir = d;
            test_sensor_dir += '/';
        }
        test_sensor_dir += "fake_sensors/";

        MqttBroker = NewFakeMqttBroker(*this);
        MqttClient = MqttBroker->MakeClient("onewire-driver-test");
        auto backend = NewDriverBackend(MqttClient);
        Driver = NewDriver(TDriverArgs{}
                               .SetId("onewire-driver-test")
                               .SetBackend(backend)
                               .SetIsTesting(true)
                               .SetReownUnknownDevices(true)
                               .SetUseStorage(false));

        Driver->StartLoop();
    }
    void TearDown()
    {
        Driver->StopLoop();
        TLoggedFixture::TearDown();
    }

    PFakeMqttBroker MqttBroker;
    PFakeMqttClient MqttClient;
    PDeviceDriver Driver;
};

TEST_F(TOnewireDriverTest, create_and_read)
{
    TOneWireDriverWorker w1_driver(DeviceId, Driver, Info, Debug, Error, test_sensor_dir + "1_sensor/");
    w1_driver.RunIteration();
    Emit() << "Clear()";
}

TEST_F(TOnewireDriverTest, move_and_read)
{
    TOneWireDriverWorker w1_driver(DeviceId, Driver, Info, Debug, Error, test_sensor_dir + "1_sensor/");
    w1_driver.RunIteration();
    Emit() << "Moving sensor to tmp";
    rename((test_sensor_dir + string("1_sensor/w1_bus_master1/28-00000a013d97")).c_str(),
           (test_sensor_dir + string("1_sensor/w1_bus_master1/tmp-28-00000a013d97")).c_str());
    w1_driver.RunIteration();
    Emit() << "Moving sensor back";
    rename((test_sensor_dir + string("1_sensor/w1_bus_master1/tmp-28-00000a013d97")).c_str(),
           (test_sensor_dir + string("1_sensor/w1_bus_master1/28-00000a013d97")).c_str());
    w1_driver.RunIteration();
    Emit() << "Clear()";
}

TEST_F(TOnewireDriverTest, bulk_read)
{
    std::ofstream f;
    f.open(test_sensor_dir + "2_buses/w1_bus_master2/therm_bulk_read", std::ofstream::trunc);
    f << "1";
    f.close();
    TOneWireDriverWorker w1_driver(DeviceId, Driver, Info, Debug, Error, test_sensor_dir + "2_buses/");
    w1_driver.RunIteration();
    Emit() << "Clear()";
}