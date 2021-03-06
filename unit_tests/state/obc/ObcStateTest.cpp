#include <string.h>
#include <algorithm>
#include <chrono>
#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"
#include "mock/StorageAccessMock.hpp"
#include "obc/ObcState.hpp"
#include "state/struct.h"

using testing::Invoke;
using testing::Eq;
using testing::_;

using namespace std::chrono_literals;
namespace
{
    class ObcStateTest : public testing::Test
    {
      protected:
        StorageAccessMock storage;
        state::SystemPersistentState stateObject;
    };

    TEST_F(ObcStateTest, TestReadingStateInvalidForwardSignagure)
    {
        EXPECT_CALL(storage, Read(8, _)).WillOnce(Invoke([](std::uint32_t, gsl::span<std::uint8_t> buffer) {
            memset(buffer.data(), 0x00, buffer.size());
            const auto size = buffer.size();
            buffer[size - 4u] = 0xee;
            buffer[size - 3u] = 0x77;
            buffer[size - 2u] = 0xaa;
            buffer[size - 1u] = 0x55;
        }));

        ASSERT_FALSE(obc::ReadPersistentState(this->stateObject, 8, this->storage));
    }

    TEST_F(ObcStateTest, TestReadingStateInvalidEndSignature)
    {
        EXPECT_CALL(storage, Read(8, _)).WillOnce(Invoke([](std::uint32_t, gsl::span<std::uint8_t> buffer) {
            memset(buffer.data(), 0x00, buffer.size());
            buffer[0] = 0xee;
            buffer[1] = 0x77;
            buffer[2] = 0xaa;
            buffer[3] = 0x55;
        }));

        ASSERT_FALSE(obc::ReadPersistentState(this->stateObject, 8, this->storage));
    }
}
