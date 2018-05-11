#include <gtest/gtest.h>
#include "testlog.h"
#include "sysfs_w1.h"

using namespace std;

class TMaybeValueTest: public TLoggedFixture
{
    protected:
        void SetUp() {};
        void TearDown() {};

};

TEST_F(TMaybeValueTest, false_object_1)
{
    auto v = TMaybeValue<int>();
    EXPECT_EQ(v.IsDefined(), false);
}

TEST_F(TMaybeValueTest, false_object_2)
{
    auto v = TMaybeValue<int>(12);
    EXPECT_EQ(v.IsDefined(), true);
}

TEST_F(TMaybeValueTest, false_object_3)
{
    auto v = TMaybeValue<int>(12);
    EXPECT_EQ(v.GetValue(), 12);
}