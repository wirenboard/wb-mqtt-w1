#pragma once

#include <stdexcept>
#include <string>
#include <vector>

/**
 * @brief 1-Wire thermometer class
 *
 */
class TSysfsOneWireThermometer
{
public:
    /**
     * @brief Construct a new TSysfsOneWireThermometer object
     *
     * @param id unique identifier code of the thermometer, usually in form 28-00000a013d97
     * @param dir directory holding thermometer's folder in sysfs, usually /sys/bus/w1/devices/
     */
    TSysfsOneWireThermometer(const std::string& id, const std::string& dir);

    /**
     * @brief Get the Id object
     *
     * @return const std::string& thermometer's identifier code, got in constructor
     */
    const std::string& GetId() const;

    /**
     * @brief Read temperature from sysfs. Throws TOneWireReadErrorException if readed value is
     * incorrect.
     *
     * @return double readed temperature in Celsius degrees
     */
    double ReadTemperature() const;

    bool operator==(const TSysfsOneWireThermometer& val) const;

private:
    std::string Id;
    std::string DeviceFileName;
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
     * @param devicesDir directory holding thermometer's folders in sysfs, usually /sys/bus/w1/devices/
     */
    TSysfsOneWireManager(const std::string& devicesDir);

    /**
     * @brief Perform devices discovery
     * 
     */
    void RescanBus();

    /**
     * @brief Get the Devices object
     * 
     * @return const std::vector<TSysfsOneWireThermometer>& get found 1-Wire thermometers
     */
    const std::vector<TSysfsOneWireThermometer>& GetDevices() const;

private:
    std::string                           DevicesDir;
    std::vector<TSysfsOneWireThermometer> Devices;
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
