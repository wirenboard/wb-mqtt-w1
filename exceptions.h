#pragma once

#include <wblib/exceptions.h>

class TW1SensorDriverException: public WBMQTT::TBaseException
{
public:
    TW1SensorDriverException(const char * file, int line, const std::string & message);
    ~TW1SensorDriverException() noexcept = default;
};
