#pragma once
#include <string>
#include <map>
#include <dirent.h>
#include <wblib/utils.h>
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include "log.h"
using namespace std;

const string SysfsOnewireDevicesPath = "/sys/bus/w1/devices/";

enum class TOnewireFamilyType
{
    ProgResThermometer = 0x28,
    Unknown = 0x00,
};

template<class T>
class TMaybeValue {
        bool isDefinedValue;
        T value;
    public:
        TMaybeValue () {
            isDefinedValue = false;
        }
        TMaybeValue (T v) {
            isDefinedValue = true;
            value = v;            
        }

        const bool IsDefined() {return isDefinedValue;}
        const T GetValue() {return value;}

};
#define NotDefinedMaybe TMaybeValue<double>()

class TSysfsOnewireDevice
{
public:
    TSysfsOnewireDevice(const string& device_name);

    inline TOnewireFamilyType GetDeviceFamily() const {return Family;};
    inline const string & GetDeviceId() const {return DeviceId;};
    inline const string & GetDeviceName() const {return DeviceName;};
    TMaybeValue<double> ReadTemperature() const;

    friend bool operator== (const TSysfsOnewireDevice & first, const TSysfsOnewireDevice & second);
private:
    string DeviceName;
    TOnewireFamilyType Family;
    string DeviceId;
    string DeviceDir;


};


class TSysfsOnewireManager
{
public:
    TSysfsOnewireManager()  {};

    void RescanBus();

    const vector<TSysfsOnewireDevice>& GetDevicesP() const;
    void ClearDevices(){ Devices.clear();}
private:
    // FIXME: once found, device will be kept indefinetely
    // consider some kind of ref-counting smart vectors instead

    vector<TSysfsOnewireDevice> Devices;

};


class TOneWireReadErrorException : public std::exception
{
public:
   TOneWireReadErrorException(const std::string& deviceFileName) : Message("1-Wire system : error reading value from ") {Message.append(deviceFileName);}
   virtual ~TOneWireReadErrorException() throw() {}
   virtual const char* what() const throw() {return Message.c_str();}
protected:
   std::string Message;
};

class TOneWireWriteErrorException : public std::exception
{
public:
   TOneWireWriteErrorException(const std::string& deviceFileName) : Message("1-Wire system : error writing value from ") {Message.append(deviceFileName);}
   virtual ~TOneWireWriteErrorException() throw() {}
   virtual const char* what() const throw() {return Message.c_str();}
protected:
   std::string Message;
};
