#include <gtest/gtest.h>
#include "testlog.h"
#include "sysfs_w1.h"

using namespace std;

    #undef SysfsOnewireDevicesPath
    #define SysfsOnewireDevicesPath string("./fake_sensors/no_sensor/")

class TMaybeValueTest: public TLoggedFixture
{
    protected:
        void SetUp() {};
        void TearDown() {};
};

TEST_F(TMaybeValueTest, false_object_1)
{
    auto v = TMaybeValue<int>();
    EXPECT_EQ(v.IsDefined(), false);
}

TEST_F(TMaybeValueTest, true_object_1)
{
    auto v = TMaybeValue<int>(12);
    EXPECT_EQ(v.IsDefined(), true);
}

TEST_F(TMaybeValueTest, true_object_2)
{
    auto v = TMaybeValue<int>(12);
    EXPECT_EQ(v.GetValue(), 12);
}

//////// SysfsOnewireDevice test

class TSysfsOnewireDeviceTest: public TLoggedFixture
{
    protected:
        void SetUp() {};
        void TearDown() {};
};

TEST_F(TSysfsOnewireDeviceTest, family)
{
    TSysfsOnewireDevice s = TSysfsOnewireDevice("sensor_name", "asd");
    EXPECT_EQ(s.GetDeviceFamily(), TOnewireFamilyType::ProgResThermometer);
}

TEST_F(TSysfsOnewireDeviceTest, name)
{
    TSysfsOnewireDevice s = TSysfsOnewireDevice("sensor_name", "asd");
    EXPECT_EQ(s.GetDeviceName(),"sensor_name");
}

TEST_F(TSysfsOnewireDeviceTest, operator_true)
{
    TSysfsOnewireDevice s1 = TSysfsOnewireDevice("sensor_name", "asd");
    TSysfsOnewireDevice s2 = TSysfsOnewireDevice("sensor_name", "asd");
    EXPECT_EQ(s1, s2);
}

TEST_F(TSysfsOnewireDeviceTest, operator_false)
{
    TSysfsOnewireDevice s1 = TSysfsOnewireDevice("sensor_name1", "asd");
    TSysfsOnewireDevice s2 = TSysfsOnewireDevice("sensor_name2", "asd");
    EXPECT_EQ( (s1 == s2), false );
}

//////// SysfsOnewireManager test

class TSysfsOnewireManagerTest: public TLoggedFixture
{
    protected:
        void SetUp() {};
        void TearDown() {};
};

TEST_F(TSysfsOnewireManagerTest, no_sensor)
{ 
    TSysfsOnewireManager manager = TSysfsOnewireManager("./fake_sensors/no_sensor/");
    manager.RescanBus();
    EXPECT_EQ(manager.GetDevicesP().size(), 0);
} 

TEST_F(TSysfsOnewireManagerTest, 1_sensor)
{ 
    TSysfsOnewireManager manager = TSysfsOnewireManager("./fake_sensors/1_sensor/");
    manager.RescanBus();
    EXPECT_EQ(manager.GetDevicesP().size(), 1);
} 

TEST_F(TSysfsOnewireManagerTest, 2_sensor)
{ 
    TSysfsOnewireManager manager = TSysfsOnewireManager("./fake_sensors/2_sensor/");
    manager.RescanBus();
    EXPECT_EQ(manager.GetDevicesP().size(), 2);
} 