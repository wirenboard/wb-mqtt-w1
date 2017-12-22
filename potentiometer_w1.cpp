#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include "sysfs_w1.h"

TPotentiometerOnewireDevice::TPotentiometerOnewireDevice(const string& device_name)
    : TSysfsOnewireDevice(device_name)
{
    Family = TOnewireFamilyType::ProgResPotentiometer;
    DeviceMQTTParams.push_back("/meta/type");
    DeviceMQTTParams.push_back("range");
    DeviceMQTTParams.push_back("/meta/order");
    DeviceMQTTParams.push_back("2");
    DeviceMQTTParams.push_back("/meta/max");
    DeviceMQTTParams.push_back("12345");
}

TMaybe<float> TPotentiometerOnewireDevice::Read() const
{
    return (float) 10;
}
