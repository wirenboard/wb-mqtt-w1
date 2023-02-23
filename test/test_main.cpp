#include <limits.h>
#include "gtest/gtest.h"
#include <wblib/testing/testlog.h>

using WBMQTT::Testing::TLoggedFixture;

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
