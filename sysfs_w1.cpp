#include "sysfs_w1.h"

#define LOG(logger) ::logger.Log() << "[w1 device] "


using namespace WBMQTT;

template<typename T>
void UnorderedVectorDifference(const vector<T> &first, const vector<T>& second, vector<T> & result)
{
    for (auto & el_first: first) {
        bool found = false;
        for (auto & el_second: second) {
            if (el_first == el_second) {
                found = true;
                break;
            }
        }

        if (!found) {
            result.push_back(el_first);
        }
    }
}

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
        while (!file.eof()) {
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

        LOG(Info) << "Successful error at: " << fileName;
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

    vector<TSysfsOnewireDevice> new_channels;
    UnorderedVectorDifference(current_channels, Devices, new_channels);

    vector<TSysfsOnewireDevice> absent_channels;
    UnorderedVectorDifference(Devices, current_channels, absent_channels);


    Devices.swap(current_channels);

    for (const TSysfsOnewireDevice& device: new_channels) {

        //Publish(NULL, GetChannelTopic(device) + "/meta/type", "temperature", 0, true);
    }

    //delete retained messages for absent channels
    for (const TSysfsOnewireDevice& device: absent_channels) {
        //Publish(NULL, GetChannelTopic(device) + "/meta/type", "", 0, true);
        //Publish(NULL, GetChannelTopic(device), "", 0, true);
    }
}

const vector<TSysfsOnewireDevice>& TSysfsOnewireManager::GetDevicesP() const
{
    return Devices;
}
