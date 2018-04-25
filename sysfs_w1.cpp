#include "sysfs_w1.h"

#define LOG(logger) ::logger.Log() << "[w1 device] "


using namespace WBMQTT;

bool operator== (const TSysfsOnewireDevice & first, const TSysfsOnewireDevice & second)
{
    return first.DeviceName == second.DeviceName;
}


TSysfsOnewireDevice::TSysfsOnewireDevice(const string& device_name)
    : DeviceName(device_name)
{
    //FIXME: fill family number
    Family = TOnewireFamilyType::ProgResThermometer;
    DeviceId = DeviceName;
    DeviceDir = SysfsOnewireDevicesPath + DeviceName;
}

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

        for (int lines_num; i < 2; i++) {

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

void TSysfsOnewireManager::RescanBus()
{
    vector<TSysfsOnewireDevice> current_channels;

    DIR *dir;
    struct dirent *ent;
    string entry_name;
    if ((dir = opendir (SysfsOnewireDevicesPath.c_str())) != NULL) {
        /* print all the files and directories within directory */
        while ((ent = readdir (dir)) != NULL) {
            printf ("%s\n", ent->d_name);
            entry_name = ent->d_name;
            if (StringStartsWith(entry_name, "28-") ||
                StringStartsWith(entry_name, "10-") ||
                StringStartsWith(entry_name, "22-") )
            {
                    current_channels.emplace_back(entry_name);
            }
        }
        closedir (dir);
    } else {
        cerr << "ERROR: could not open directory " << SysfsOnewireDevicesPath << endl;
    }

    Devices.swap(current_channels);
}

const vector<TSysfsOnewireDevice>& TSysfsOnewireManager::GetDevicesP() const
{
    return Devices;
}
