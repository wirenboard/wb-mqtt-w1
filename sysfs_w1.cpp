#include "sysfs_w1.h"

#define LOG(logger) ::logger.Log() << "[w1 device] "


using namespace WBMQTT;

bool operator== (const TSysfsOnewireDevice & first, const TSysfsOnewireDevice & second)
{
    return (first.DeviceName == second.DeviceName) && (first.Family == second.Family);
}

/*  @brief  class constructor, it fills sensor data parameters
 *  @param  device_name:    device name string what also determins the file descriptor of sensor
 */

TSysfsOnewireDevice::TSysfsOnewireDevice(const string& device_name, const string& dir) : DeviceName(device_name), Family(TOnewireFamilyType::ProgResThermometer)
{
    DeviceDir = dir + DeviceName;
}

/*  @brief  reading temperature from device
 *  @retval TMaybeValue object, if reading was not successful, TMaybeValue is not defined
 */

TMaybeValue<double> TSysfsOnewireDevice::ReadTemperature() const
{
    std::string data;
    bool bFoundCrcOk=false;

    static const std::string tag("t=");

    std::ifstream file;
    std::string fileName=DeviceDir +"/w1_slave";
    file.open(fileName.c_str());
    if (file.is_open()) {
        std::string sLine;
        /*  reading file till eof could lead to a stuck 
            when device is removed */


        for (int lines_num = 0; lines_num < 2; lines_num++) {
            getline(file, sLine);
            size_t tpos;
            if (sLine.find("crc=")!=std::string::npos) {
                if (sLine.find("YES")!=std::string::npos) {
                    bFoundCrcOk=true;
                }
            } else if ((tpos=sLine.find(tag))!=std::string::npos) {
                data = sLine.substr(tpos+tag.length());
            }
        }
        file.close();
    }

    if (bFoundCrcOk) {
        LOG(Info) << "CRC OK" << fileName;

        int data_int = std::stoi(data);

        if (data_int == 85000) {
            // wrong read
            LOG(Info) << "Reading error at: " << fileName;
            return NotDefinedMaybe;
        }

        if (data_int == 127937) {
            // returned max possible temp, probably an error
            // (it happens for chineese clones)
            LOG(Info) << "Returns with max value (error): " << fileName;
            return NotDefinedMaybe;
        }

        LOG(Info) << "Successful read at: " << fileName;
        return TMaybeValue<double>(data_int/1000.0f); // Temperature given by kernel is in thousandths of degrees
    }

    return NotDefinedMaybe;
}

/*  @brief  rescanning the available sensors on the bus and resfreshing "Devices" list
 */

void TSysfsOnewireManager::RescanBus()
{
    vector<TSysfsOnewireDevice> current_channels;

    DIR *dir;
    struct dirent *ent;
    string entry_name;
        /* print all the files and directories within directory */
    if ((dir = opendir (devices_dir.c_str())) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            entry_name = ent->d_name;
            if (StringStartsWith(entry_name, "28-") ||
                StringStartsWith(entry_name, "10-") ||
                StringStartsWith(entry_name, "22-") )
            {
                    current_channels.emplace_back(entry_name, devices_dir);
            }
        }
        closedir (dir);
    } else {
        cerr << "ERROR: could not open directory " << devices_dir << endl;
    }

    Devices.swap(current_channels);
}

/*  @brief  get function of "Devices" list
 *  @retval Devices list reference
 */

const vector<TSysfsOnewireDevice>& TSysfsOnewireManager::GetDevicesP() const
{
    return Devices;
}
