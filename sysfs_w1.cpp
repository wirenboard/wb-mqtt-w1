#include "sysfs_w1.h"

#include "file_utils.h"
#include <fstream>
#include <wblib/utils.h>

/*  @brief  class constructor, it fills sensor data parameters
 *  @param  device_name:    device name string what also determins the file descriptor of sensor
 */

TSysfsOneWireThermometer::TSysfsOneWireThermometer(const std::string& id,
                                                   const std::string& deviceDir)
    : Id(id)
{
    DeviceFileName = deviceDir + Id + "/w1_slave";
}

/*  @brief  reading temperature from device
 *  @retval TMaybeValue object, if reading was not successful, TMaybeValue is not defined
 */

double TSysfsOneWireThermometer::ReadTemperature() const
{
    std::string data;
    bool        crcOk = false;

    const std::string tag("t=");

    std::ifstream file;
    OpenWithException(file, DeviceFileName);

    /*  reading file till eof could lead to a stuck
        when device is removed */
    while (file.good()) {
        std::string sLine;
        std::getline(file, sLine);
        if (sLine.find("crc=") != std::string::npos) {
            if (sLine.find("YES") != std::string::npos) {
                crcOk = true;
            }
        } else {
            size_t tpos = sLine.find(tag);
            if (tpos != std::string::npos) {
                data = sLine.substr(tpos + tag.length());
            }
        }
    }

    if (!crcOk) {
        throw TOneWireReadErrorException("Bad CRC", DeviceFileName);
    }

    if (data.empty()) {
        throw TOneWireReadErrorException("Can't read temperature", DeviceFileName);
    }

    int dataInt = std::stoi(data);

    // Thermometer can't measure themperature
    if (dataInt == 85000) {
        throw TOneWireReadErrorException("Measurement error", DeviceFileName);
    }

    // returned max possible temp, probably an error (it happens for chineese clones)
    if (dataInt == 127937) {
        throw TOneWireReadErrorException("Thermometer error", DeviceFileName);
    }

    return dataInt / 1000.0; // Temperature given by kernel is in thousandths of degrees
}

bool TSysfsOneWireThermometer::operator==(const TSysfsOneWireThermometer& val) const
{
    return (Id == val.Id);
}

const std::string& TSysfsOneWireThermometer::GetId() const
{
    return Id;
}

TSysfsOneWireManager::TSysfsOneWireManager(const std::string& devicesDir) : DevicesDir(devicesDir) {}

/*  @brief  rescanning the available sensors on the bus and resfreshing "Devices" list
 */
void TSysfsOneWireManager::RescanBus()
{
    std::vector<TSysfsOneWireThermometer> currentChannels;

    auto prefixes = {"28-", "10-", "22-"};
    IterateDir(DevicesDir, [&](const auto& name) {
        for (const auto& prefix : prefixes) {
            if (WBMQTT::StringStartsWith(name, prefix)) {
                currentChannels.emplace_back(name, DevicesDir);
            }
        }
        return false;
    });

    Devices.swap(currentChannels);
}

/*  @brief  get function of "Devices" list
 *  @retval Devices list reference
 */
const std::vector<TSysfsOneWireThermometer>& TSysfsOneWireManager::GetDevices() const
{
    return Devices;
}

TOneWireReadErrorException::TOneWireReadErrorException(const std::string& message,
                                                       const std::string& deviceFileName)
    : std::runtime_error(message + " (" + deviceFileName + ")")
{
}