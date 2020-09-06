#include "sysfs_w1.h"
#include <gtest/gtest.h>
#include <wblib/testing/testlog.h>

using namespace std;
using namespace WBMQTT;
using namespace WBMQTT::Testing;

//////// SysfsOnewireDevice test

class TSysfsOnewireDeviceTest : public TLoggedFixture
{
protected:
    string test_sensor_root_dir;

    void SetUp()
    {
        char* d = getenv("TEST_DIR_ABS");
        if (d != NULL) {
            test_sensor_root_dir = d;
            test_sensor_root_dir += '/';
        }
        test_sensor_root_dir += "fake_sensors/";
    };
};

TEST_F(TSysfsOnewireDeviceTest, name)
{
    TSysfsOneWireThermometer s = TSysfsOneWireThermometer("sensor_name", "asd");
    EXPECT_EQ(s.GetId(), "sensor_name");
}

TEST_F(TSysfsOnewireDeviceTest, operator_true)
{
    TSysfsOneWireThermometer s1 = TSysfsOneWireThermometer("sensor_name", "asd");
    TSysfsOneWireThermometer s2 = TSysfsOneWireThermometer("sensor_name", "asd");
    EXPECT_EQ(s1, s2);
}

TEST_F(TSysfsOnewireDeviceTest, operator_false)
{
    TSysfsOneWireThermometer s1 = TSysfsOneWireThermometer("sensor_name1", "asd");
    TSysfsOneWireThermometer s2 = TSysfsOneWireThermometer("sensor_name2", "asd");
    EXPECT_EQ((s1 == s2), false);
}

TEST_F(TSysfsOnewireDeviceTest, 1_sensor_read)
{
    TSysfsOneWireThermometer s1 =
        TSysfsOneWireThermometer("28-00000a013d97", test_sensor_root_dir + string("1_sensor/"));
    auto v = s1.ReadTemperature();
    EXPECT_EQ(to_string(v), "26.312000");
}

TEST_F(TSysfsOnewireDeviceTest, no_sensor_read)
{
    TSysfsOneWireThermometer s1 =
        TSysfsOneWireThermometer("28-00000a013d97", test_sensor_root_dir + string("no_sensor/"));
    ASSERT_THROW(s1.ReadTemperature(), exception);
}

TEST_F(TSysfsOnewireDeviceTest, wrong_crc)
{
    TSysfsOneWireThermometer s1 =
        TSysfsOneWireThermometer("28-00000a013000-wrong-crc",
                                 test_sensor_root_dir + string("2_sensor/"));
    ASSERT_THROW(s1.ReadTemperature(), runtime_error);
}

//////// SysfsOnewireManager test

class TSysfsOnewireManagerTest : public TLoggedFixture
{
protected:
    string test_sensor_root_dir;

    void   SetUp()
    {
        char* d = getenv("TEST_DIR_ABS");
        if (d != NULL) {
            test_sensor_root_dir = d;
            test_sensor_root_dir += '/';
        }
        test_sensor_root_dir += "fake_sensors/";
    }
};

TEST_F(TSysfsOnewireManagerTest, no_sensor)
{
    TSysfsOneWireManager manager = TSysfsOneWireManager(test_sensor_root_dir + string("no_sensor"));
    manager.RescanBus();
    EXPECT_EQ(manager.GetDevices().size(), 0);
}

TEST_F(TSysfsOnewireManagerTest, 1_sensor)
{
    TSysfsOneWireManager manager = TSysfsOneWireManager(test_sensor_root_dir + string("1_sensor"));
    manager.RescanBus();
    EXPECT_EQ(manager.GetDevices().size(), 1);
}

TEST_F(TSysfsOnewireManagerTest, 2_sensor)
{
    TSysfsOneWireManager manager = TSysfsOneWireManager(test_sensor_root_dir + string("2_sensor"));
    manager.RescanBus();
    EXPECT_EQ(manager.GetDevices().size(), 2);
}