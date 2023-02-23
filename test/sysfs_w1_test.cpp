#include "sysfs_w1.h"
#include <gtest/gtest.h>
#include <wblib/testing/testlog.h>
#include <fstream>

using namespace std;
using namespace WBMQTT;
using namespace WBMQTT::Testing;

//////// SysfsOnewireDevice test

class TSysfsOnewireDeviceTest : public TLoggedFixture
{
protected:
    string test_sensor_root_dir;

    void SetUp() override
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

TEST_F(TSysfsOnewireDeviceTest, 1_sensor_read)
{
    auto s1 = TSysfsOneWireThermometer("28-00000a013d97", test_sensor_root_dir + string("1_sensor/w1_bus_master1/"));
    auto v = s1.GetTemperature();
    EXPECT_EQ(to_string(v), "26.312000");
}

TEST_F(TSysfsOnewireDeviceTest, no_sensor_read)
{
    auto s1 = TSysfsOneWireThermometer("28-00000a013d97", test_sensor_root_dir + string("no_sensor/w1_bus_master1/"));
    ASSERT_THROW(s1.GetTemperature(), exception);
}

TEST_F(TSysfsOnewireDeviceTest, wrong_crc)
{
    auto s1 = TSysfsOneWireThermometer("28-00000a013000-wrong-crc",
                                       test_sensor_root_dir + string("2_sensor/w1_bus_master1/"));
    ASSERT_THROW(s1.GetTemperature(), runtime_error);
}

//////// SysfsOnewireManager test

class TSysfsOnewireManagerTest : public TLoggedFixture
{
protected:
    string test_sensor_root_dir;

    void SetUp() override
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
    auto m = TSysfsOneWireManager(test_sensor_root_dir + string("no_sensor/"), Debug, Error);
    EXPECT_EQ(m.RescanBusAndRead().size(), 0);
}

TEST_F(TSysfsOnewireManagerTest, 1_sensor)
{
    auto m = TSysfsOneWireManager(test_sensor_root_dir + string("1_sensor/"), Debug, Error);
    EXPECT_EQ(m.RescanBusAndRead().size(), 1);
}

TEST_F(TSysfsOnewireManagerTest, 2_sensor)
{
    auto m = TSysfsOneWireManager(test_sensor_root_dir + string("2_sensor/"), Debug, Error);
    EXPECT_EQ(m.RescanBusAndRead().size(), 2);
}

TEST_F(TSysfsOnewireManagerTest, 2_buses)
{
    std::ofstream f;
    f.open(test_sensor_root_dir + "2_buses/w1_bus_master2/therm_bulk_read", std::ofstream::trunc);
    f << "1";
    f.close();
    auto m = TSysfsOneWireManager(test_sensor_root_dir + string("2_buses/"), Debug, Error);
    EXPECT_EQ(m.RescanBusAndRead().size(), 2);
}