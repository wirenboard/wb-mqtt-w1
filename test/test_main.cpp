#include "gtest/gtest.h"
#include <limits.h>
#include <wblib/testing/testlog.h>

using WBMQTT::Testing::TLoggedFixture;

int main(int argc, char** argv)
{
    TLoggedFixture::SetExecutableName(argv[0]);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
