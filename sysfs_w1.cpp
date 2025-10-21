#include "sysfs_w1.h"

#include "file_utils.h"
#include <algorithm>
#include <fcntl.h>
#include <fstream>
#include <unistd.h>
#include <wblib/utils.h>

using namespace std::chrono;

#define LOG(logger) logger.Log() << "[w1 driver] "

namespace
{
    const char BULK_CONVERSION_TRIGGER[] = "trigger";
    const auto MAX_CONVERSION_TIME = milliseconds(2000);
    const auto MAX_VALUE_CHANGE = 10 * 1000;    // 1 degree per second for DEFAULT_POLL_INTERVALL_MS
    const auto MEASUREMENT_ERROR_VALUE = 85000; // sensor power on temperature value (read without conversion)
    const auto MEASUREMENT_MAX_VALUE = 127937;  // max possible temperature value (for some chineese clones)

    std::unordered_map<std::string, int> lastValueMap;

    template<class T, class Pred> void erase_if(T& c, Pred pred)
    {
        auto last = c.end();
        for (auto i = c.begin(); i != last;) {
            if (pred(*i)) {
                i = c.erase(i);
            } else {
                ++i;
            }
        }
    }

    std::string ReadLine(const std::string& fileName)
    {
        std::ifstream file;
        OpenWithException(file, fileName);
        std::string str;
        std::getline(file, str);
        return str;
    }

    bool RunBulkRead(const std::string& fileName, WBMQTT::TLogger& logger)
    {
        auto fd = open(fileName.c_str(), O_WRONLY | O_APPEND);
        if (fd < 0) {
            LOG(logger) << "Can't open file:" << fileName;
            return false;
        }
        auto s = write(fd, BULK_CONVERSION_TRIGGER, sizeof(BULK_CONVERSION_TRIGGER)); // Write trailing zero
        close(fd);
        return (s == sizeof(BULK_CONVERSION_TRIGGER));
    }

    double GetTemperatureFromString(const std::string& str, const std::string& deviceFileName)
    {
        if (str.empty()) {
            throw TOneWireReadErrorException("Can't read temperature", deviceFileName);
        }

        int dataInt = std::stoi(str.c_str());

        // Thermometer can't measure temperature?
        if (dataInt == MEASUREMENT_ERROR_VALUE) {
            auto it = lastValueMap.find(deviceFileName);
            if (it == lastValueMap.end() || abs(dataInt - it->second) > MAX_VALUE_CHANGE) {
                throw TOneWireReadErrorException("Measurement error", deviceFileName);
            }
        }

        // returned max possible temp, probably an error (it happens for chineese clones)
        if (dataInt == MEASUREMENT_MAX_VALUE) {
            throw TOneWireReadErrorException("Thermometer error", deviceFileName);
        }

        lastValueMap[deviceFileName] = dataInt;
        return dataInt / 1000.0; // Temperature given by kernel is in thousandths of degrees
    }

    double GetBulkReadTemperature(const std::string& deviceFileName)
    {
        return GetTemperatureFromString(ReadLine(deviceFileName), deviceFileName);
    }

    double GetDirectConvertedTemperature(const std::string& deviceFileName)
    {
        std::string data;
        bool crcOk = false;

        const std::string tag("t=");

        std::ifstream file;
        OpenWithException(file, deviceFileName);

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
            throw TOneWireReadErrorException("Bad CRC", deviceFileName);
        }

        return GetTemperatureFromString(data, deviceFileName);
    }

    struct TBusMaster
    {
        std::string Dir;
        bool SupportsBulkRead;
    };
}

TSysfsOneWireThermometer::TSysfsOneWireThermometer(const std::string& id, const std::string& dir, bool bulkRead)
    : Id(id),
      Status(TSysfsOneWireThermometer::New),
      BulkRead(bulkRead)
{
    SetDeviceFileName(dir);
}

void TSysfsOneWireThermometer::SetDeviceFileName(const std::string& dir)
{
    DeviceFileName = dir + "/" + Id + (BulkRead ? "/temperature" : "/w1_slave");
}

double TSysfsOneWireThermometer::GetTemperature() const
{
    if (BulkRead) {
        return GetBulkReadTemperature(DeviceFileName);
    }
    return GetDirectConvertedTemperature(DeviceFileName);
}

const std::string& TSysfsOneWireThermometer::GetId() const
{
    return Id;
}

TSysfsOneWireThermometer::PresenceStatus TSysfsOneWireThermometer::GetStatus() const
{
    return Status;
}

void TSysfsOneWireThermometer::MarkAsDisconnected()
{
    Status = Disconnected;
}

bool TSysfsOneWireThermometer::FoundAgain(const std::string& dir)
{
    Status = Connected;
    if (WBMQTT::StringStartsWith(DeviceFileName, dir)) {
        return true;
    }
    SetDeviceFileName(dir);
    return false;
}

TSysfsOneWireManager::TSysfsOneWireManager(const std::string& devicesDir,
                                           WBMQTT::TLogger& debugLogger,
                                           WBMQTT::TLogger& errorLogger)
    : DevicesDir(devicesDir),
      DebugLogger(debugLogger),
      ErrorLogger(errorLogger)
{}

std::vector<std::shared_ptr<TSysfsOneWireThermometer>> TSysfsOneWireManager::RescanBusAndRead()
{
    const auto prefixes = {"28-", "10-", "22-"};
    erase_if(Devices, [](const auto& it) { return it.second->GetStatus() == TSysfsOneWireThermometer::Disconnected; });
    for (auto& d: Devices) {
        d.second->MarkAsDisconnected();
    }

    std::vector<TBusMaster> busMasters;

    IterateDir(DevicesDir, [&](const auto& name) {
        if (!WBMQTT::StringStartsWith(name, "w1_bus_master")) {
            return false;
        }
        TBusMaster bm;
        bm.Dir = DevicesDir + name;
        bm.SupportsBulkRead = (access((bm.Dir + "/therm_bulk_read").c_str(), F_OK) == 0);
        busMasters.push_back(bm);

        if (bm.SupportsBulkRead) {
            RunBulkRead(bm.Dir + "/therm_bulk_read", ErrorLogger);
        }

        IterateDir(bm.Dir, [&](const auto& name) {
            for (const auto& prefix: prefixes) {
                if (WBMQTT::StringStartsWith(name, prefix)) {
                    auto it = Devices.find(name);
                    if (it == Devices.end()) {
                        it =
                            Devices
                                .insert({name,
                                         std::make_shared<TSysfsOneWireThermometer>(name, bm.Dir, bm.SupportsBulkRead)})
                                .first;
                    } else {
                        if (!it->second->FoundAgain(bm.Dir)) {
                            LOG(DebugLogger) << name << " is switched to " << bm.Dir;
                        }
                    }
                }
            }
            return false;
        });
        return false;
    });

    auto time = steady_clock::now();
    bool inConversion = true;
    while (inConversion && (duration_cast<milliseconds>(steady_clock::now() - time) < MAX_CONVERSION_TIME)) {
        inConversion = false;
        for (auto& bm: busMasters) {
            if (bm.SupportsBulkRead) {
                try {
                    auto status = ReadLine(bm.Dir + "/therm_bulk_read");
                    inConversion |= (status.empty() || status[0] != '1');
                } catch (const std::exception& e) {
                    LOG(ErrorLogger) << e.what();
                }
            }
        }
        if (inConversion) {
            std::this_thread::sleep_for(milliseconds(100));
        }
    }
    if (inConversion) {
        LOG(DebugLogger) << "Conversion takes too much time";
    }

    std::vector<std::shared_ptr<TSysfsOneWireThermometer>> res;
    for (auto& d: Devices) {
        res.push_back(d.second);
    }
    std::sort(res.begin(), res.end(), [](const auto& v1, const auto& v2) { return v1->GetId() < v2->GetId(); });
    return res;
}

TOneWireReadErrorException::TOneWireReadErrorException(const std::string& message, const std::string& deviceFileName)
    : std::runtime_error(message + " (" + deviceFileName + ")")
{}