#include "exceptions.h"

using namespace std;
using namespace WBMQTT;

TW1SensorDriverException::TW1SensorDriverException(const char * file, int line, const string & message)
    : TBaseException(file, line, message)
{}
