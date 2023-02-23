#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

#include <wblib/log.h>

/**
 * @brief 1-Wire thermometer class
 *
 */
class TSysfsOneWireThermometer
{
public:
    enum PresenceStatus
    {
        New,           // the thermometer was found during last search cycle
        Connected,     // the thermometer is connected and was connected during last search cycle
        Disconnected   // the thermometer was not found during last search cycle
    };

    /**
     * @brief Construct a new TSysfsOneWireThermometer object
     *
     * @param id unique identifier code of the thermometer, usually in form 28-00000a013d97
     * @param dir directory holding thermometer's folder in sysfs, usually /sys/bus/w1/devices/w1_bus_masterX
     * @param bulkRead true - use 'temperature' sysfs entry, false - use 'w1_slave' sysfs entry
     */
    TSysfsOneWireThermometer(const std::string& id, const std::string& dir, bool bulkRead = false);

    /**
     * @brief Get the Id object
     *
     * @return const std::string& thermometer's identifier code, got in constructor
     */
    const std::string& GetId() const;

    /**
     * @brief Get temperature. Throws TOneWireReadErrorException if read value is
     * incorrect.
     *
     * @return double read temperature in Celsius degrees
     */
    double GetTemperature() const;

    /**
     * @brief Get thermometer status.
     */
    PresenceStatus GetStatus() const;

    /**
     * @brief Mark thermometer as disconnected. It can be deleted during next search cycle.
     */
    void MarkAsDisconnected();

    /**
     * @brief The thermometer is found again during search cycle.
     *        Set directory holding thermometer's folder in sysfs.
     *        The function provides hot switching between buses.
     * 
     * @param dir directory holding thermometer's folder in sysfs, usually /sys/bus/w1/devices/w1_bus_masterX.
     * @return true - the thermometer was located on the same bus
     * @return false - thermometer's bus is changed
     */
    bool FoundAgain(const std::string& dir);

private:
    void SetDeviceFileName(const std::string& dir);

    std::string    Id;
    std::string    DeviceFileName;
    PresenceStatus Status;
    bool           BulkRead;
};

/**
 * @brief The class performs 1-Wire thermometers discovery and holds a list of known devices
 *
 */
class TSysfsOneWireManager
{
public:
    /**
     * @brief Construct a new TSysfsOneWireManager object
     * 
     * @param devicesDir directory holding 1-Wire bus master files in sysfs, usually /sys/bus/w1/devices/
     */
    TSysfsOneWireManager(const std::string& devicesDir, WBMQTT::TLogger& debugLogger, WBMQTT::TLogger& errorLogger);

    /**
     * @brief Perform devices discovery and starts bulk reading if possible.
     * 
     * @return array of available thermometers, thermometers disconnected since last call have Deleted status
     */
    std::vector<std::shared_ptr<TSysfsOneWireThermometer>> RescanBusAndRead();

private:
    std::string      DevicesDir;
    WBMQTT::TLogger& DebugLogger;
    WBMQTT::TLogger& ErrorLogger;

    std::unordered_map<std::string,  std::shared_ptr<TSysfsOneWireThermometer>> Devices;
};

/**
 * @brief Error during reading temperature from 1-Wire thermometer
 * 
 */
class TOneWireReadErrorException : public std::runtime_error
{
public:
    TOneWireReadErrorException(const std::string& message, const std::string& deviceFileName);
};
