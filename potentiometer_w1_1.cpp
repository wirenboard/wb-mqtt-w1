/*
 * === DS2890 ===
 * Based on datasheet
 * useful: https://www.kernel.org/doc/Documentation/w1/w1.generic
 */
#include <fstream>
#include <string>
#include "sysfs_w1.h"

TPotentiometerOnewireDevice::TPotentiometerOnewireDevice(const string& device_name)
    : TSysfsOnewireDevice(device_name)
{
    Family = TOnewireFamilyType::ProgResPotentiometer;
    DeviceMQTTParams.push_back("/meta/type");
    DeviceMQTTParams.push_back("range");
    DeviceMQTTParams.push_back("/meta/order");
    DeviceMQTTParams.push_back("2");
    DeviceMQTTParams.push_back("/meta/max");
    // TODO this value should be red from the chip
    DeviceMQTTParams.push_back("100000");
}

TMaybe<float> TPotentiometerOnewireDevice::Read() const
{
    std::fstream file;
    std::string fileName=DeviceDir +"/rw";
    file.open(fileName.c_str());
    uint8_t result[2] = {0, 0};
    if (file.is_open()) {
        uint8_t buffer = CMD_READ_POSITION;
        file.write((char *) &buffer, 1);
        file.read((char *) &result, 2); // first byte - control register, second byte - desired value
        file.close();
    }

    return SINGLE_STEP_RESISTENCE * result[1];
}

/*
 * By datasheet write sequence is following:
 * 1) write CMD_WRITE_POSITION
 * 2) write value
 * 3) read saved value
 * 4) write CMD_RELEASE to sumbit written value is correct
 *
 * Meanwhile in w1 linux driver (https://github.com/torvalds/linux/blob/master/drivers/w1/w1.c) in function rw_write
 * before each write attempt reset is made. That makes it impossible to submit written value.
 *
 * In this implementation INC and DEC are used to set required value.
 */
TMaybe<float> TPotentiometerOnewireDevice::Write(float val) const
{
    std::string fileName=DeviceDir +"/rw";
    std::fstream file;
    file.rdbuf()->pubsetbuf(0, 0);
    file.open(fileName.c_str());

//    struct two_byte_buf { uint8_t byte1, byte2; };
//
//    uint8_t buf1;
//    two_byte_buf buf2;

    // first read current value
//    buf1 = CMD_READ_POSITION;
//    file.write((char *) &buf1, 1);
//    file.read((char *) &buf2, 2); // first byte - control register, second byte - desired value
//
//    uint8_t int_val = val / SINGLE_STEP_RESISTENCE + 0.5;
//    int delta = int_val - buf2.byte2;
//    printf("delta %u %u %u\n", int_val, buf2.byte2, delta);
//
//    if (delta != 0) {
//        uint8_t cmd = delta > 0 ? CMD_INCREMENT : CMD_DECREMENT;
//        printf("cmd %u\n", cmd);
//        if (delta < 0) {
//            delta = -delta;
//        }
//        for (auto i = 1; i <= delta; i++) {
//            printf("i %u\n", i);
//            file.write((char *) &cmd, 1);
//            file.read((char *) &buf1, 1);
//            printf("was: %u; is: %u\n", buf2.byte2, buf1);
//        }
//    }
//
//    return SINGLE_STEP_RESISTENCE * buf1;



//    uint8_t buf[2] = {0x55, 0x4c};
//    file.write((char *) &buf, 2);
//
//    file.read((char *) &rbuf, 1);
//    printf("read conf %u\n", rbuf);
//    rbuf = 0x96;
//    file.write((char *) &rbuf, 1);
//    file.read((char *) &rbuf, 1);
//    printf("read conf %u\n", rbuf);

//
//    rbuf = 0xcc;
//    file.write((char *) &rbuf, 1);
//
//
    uint8_t result = 0;
    uint8_t value = val / SINGLE_STEP_RESISTENCE + 0.5;
    printf("write value %u\n", value);
    if (file.is_open()) {
        uint8_t write_cmd[3] = {CMD_WRITE_POSITION, value, CMD_RELEASE};
        file.write((char *) &write_cmd, 3);

        uint8_t read_cmd = CMD_READ_POSITION;
        file.write((char *) &read_cmd, 1);
        file.read((char *) &result, 1);

        file.close();
    }

    return SINGLE_STEP_RESISTENCE * result;
}
