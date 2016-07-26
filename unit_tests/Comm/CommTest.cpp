#include <string>
#include <algorithm>
#include <em_i2c.h>
#include <tuple>
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "gmock/gmock-matchers.h"
#include "OsMock.hpp"
#include "comm/comm.h"
#include "os/os.hpp"
#include "system.h"

using testing::_;
using testing::Eq;
using testing::Ne;
using testing::Ge;
using testing::StrEq;
using testing::Return;
using testing::Invoke;

static const uint8_t ReceiverAddress = 0x60;
static const uint8_t TransmitterAddress = 0x62;

static const uint8_t ReceverGetTelemetry = 0x1A;
static const uint8_t ReceiverGetFrameCount = 0x21;
static const uint8_t ReceiverGetFrame = 0x22;
static const uint8_t ReceiverRemoveFrame = 0x24;
static const uint8_t ReceiverReset = 0xAA;

static const uint8_t HardwareReset = 0xAB;

static const uint8_t TransmitterSendFrame = 0x10;
static const uint8_t TransmitterSetBeacon = 0x14;
static const uint8_t TransmitterClearBeacon = 0x1f;
static const uint8_t TransmitterSetIdleState = 0x24;
static const uint8_t TransmitterGetTelemetry = 0x25;
static const uint8_t TransmitterSetBitRate = 0x28;
static const uint8_t TransmitterGetState = 0x41;
static const uint8_t TransmitterReset = 0xAA;

struct I2CMock
{
    MOCK_METHOD4(
        I2CWrite, I2C_TransferReturn_TypeDef(uint8_t address, uint8_t command, uint8_t* inData, uint16_t length));

    MOCK_METHOD6(I2CRead,
        I2C_TransferReturn_TypeDef(uint8_t address,
                     uint8_t command,
                     uint8_t* inData,
                     uint16_t inLength,
                     uint8_t* outData,
                     uint16_t outLength));
};

static I2CMock* mockPtr = NULL;

static I2C_TransferReturn_TypeDef TestI2CWrite(uint8_t address, uint8_t* inData, uint16_t length)
{
    if (mockPtr != NULL)
    {
        return mockPtr->I2CWrite(address, (length > 0 && inData != NULL) ? *inData : 0, inData, length);
    }
    else
    {
        return i2cTransferNack;
    }
}

static I2C_TransferReturn_TypeDef TestI2CRead(
    uint8_t address, uint8_t* inData, uint16_t inLength, uint8_t* outData, uint16_t outLength)
{
    if (mockPtr != NULL)
    {
        return mockPtr->I2CRead(
            address, (inLength > 0 && inData != NULL) ? *inData : 0, inData, inLength, outData, outLength);
    }
    else
    {
        return i2cTransferNack;
    }
}

static OSReset SetupComm(CommObject* comm, OSMock& system)
{
    CommLowInterface low;
    low.writeProc = TestI2CWrite;
    low.readProc = TestI2CRead;

    CommUpperInterface up;
    up.frameHandler = nullptr;
    up.frameHandlerContext = nullptr;
    auto reset = InstallProxy(&system);
    ON_CALL(system, CreateEventGroup()).WillByDefault(Return(reinterpret_cast<OSEventGroupHandle>(comm)));

    EXPECT_THAT(CommInitialize(comm, &low, &up), Eq(OSResultSuccess));
    return reset;
}

class CommTest : public testing::Test
{
  public:
    CommTest();
    ~CommTest();

  protected:
    CommObject comm;
    testing::NiceMock<OSMock> system;
    I2CMock i2c;
    OSReset reset;
};

CommTest::CommTest()
{
    reset = SetupComm(&comm, system);
    mockPtr = &i2c;
}

CommTest::~CommTest()
{
    mockPtr = nullptr;
}

TEST_F(CommTest, TestInitializationDoesNotTouchHardware)
{
    CommObject commObject;
    CommLowInterface low;
    low.writeProc = TestI2CWrite;
    low.readProc = TestI2CRead;
    CommUpperInterface up;
    up.frameHandler = nullptr;
    up.frameHandlerContext = nullptr;
    EXPECT_CALL(i2c, I2CRead(_, _, _, _, _, _)).Times(0);
    EXPECT_CALL(i2c, I2CWrite(_, _, _, _)).Times(0);
    CommInitialize(&commObject, &low, &up);
}

TEST_F(CommTest, TestInitializationAllocationFailure)
{
    CommObject commObject;
    CommLowInterface low;
    low.writeProc = TestI2CWrite;
    low.readProc = TestI2CRead;
    CommUpperInterface up;
    up.frameHandler = nullptr;
    up.frameHandlerContext = nullptr;
    EXPECT_CALL(system, CreateEventGroup()).WillOnce(Return(nullptr));
    const auto status = CommInitialize(&commObject, &low, &up);
    ASSERT_THAT(status, Ne(OSResultSuccess));
}

TEST_F(CommTest, TestInitialization)
{
    CommObject commObject;
    CommLowInterface low;
    low.writeProc = TestI2CWrite;
    low.readProc = TestI2CRead;
    CommUpperInterface up;
    up.frameHandler = nullptr;
    up.frameHandlerContext = nullptr;
    const auto status = CommInitialize(&commObject, &low, &up);
    ASSERT_THAT(status, Eq(OSResultSuccess));
}

TEST_F(CommTest, TestHardwareReset)
{
    EXPECT_CALL(i2c, I2CWrite(ReceiverAddress, HardwareReset, Ne(nullptr), 1)).WillOnce(Return(i2cTransferDone));
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterReset, Ne(nullptr), 1)).WillOnce(Return(i2cTransferDone));
    EXPECT_CALL(i2c, I2CWrite(ReceiverAddress, ReceiverReset, Ne(nullptr), 1)).WillOnce(Return(i2cTransferDone));
    const auto status = CommReset(&comm);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestHardwareResetFailureOnHardware)
{
    EXPECT_CALL(i2c, I2CWrite(ReceiverAddress, HardwareReset, _, 1)).WillOnce(Return(i2cTransferNack));
    const auto status = CommReset(&comm);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestHardwareResetFailureOnReceiver)
{
    EXPECT_CALL(i2c, I2CWrite(ReceiverAddress, HardwareReset, Ne(nullptr), 1)).WillOnce(Return(i2cTransferDone));
    EXPECT_CALL(i2c, I2CWrite(ReceiverAddress, ReceiverReset, Ne(nullptr), 1)).WillOnce(Return(i2cTransferNack));
    ON_CALL(i2c, I2CWrite(TransmitterAddress, _, _, _)).WillByDefault(Return(i2cTransferDone));
    const auto status = CommReset(&comm);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestHardwareResetFailureOnTransmitter)
{
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterReset, _, 1)).WillOnce(Return(i2cTransferNack));
    EXPECT_CALL(i2c, I2CWrite(ReceiverAddress, _, _, _)).WillRepeatedly(Return(i2cTransferDone));
    const auto status = CommReset(&comm);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestTransmitterReset)
{
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterReset, Ne(nullptr), 1)).WillOnce(Return(i2cTransferDone));
    const auto status = CommResetTransmitter(&comm);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestTransmitterResetFailure)
{
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterReset, _, 1)).WillOnce(Return(i2cTransferNack));
    const auto status = CommResetTransmitter(&comm);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestReceiverReset)
{
    EXPECT_CALL(i2c, I2CWrite(ReceiverAddress, ReceiverReset, Ne(nullptr), 1)).WillOnce(Return(i2cTransferDone));
    const auto status = CommResetReceiver(&comm);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestReceiverResetFailure)
{
    EXPECT_CALL(i2c, I2CWrite(ReceiverAddress, ReceiverReset, _, 1)).WillOnce(Return(i2cTransferNack));
    const auto status = CommResetReceiver(&comm);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestFrameRemoval)
{
    EXPECT_CALL(i2c, I2CWrite(ReceiverAddress, ReceiverRemoveFrame, Ne(nullptr), 1)).WillOnce(Return(i2cTransferDone));
    const auto status = CommRemoveFrame(&comm);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestFrameRemovalFailure)
{
    EXPECT_CALL(i2c, I2CWrite(ReceiverAddress, ReceiverRemoveFrame, _, 1)).WillOnce(Return(i2cTransferNack));
    const auto status = CommRemoveFrame(&comm);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestGetFrameCountFailure)
{
    EXPECT_CALL(i2c, I2CRead(ReceiverAddress, ReceiverGetFrameCount, _, _, _, _)).WillOnce(Return(i2cTransferNack));
    const auto result = CommGetFrameCount(&comm);
    ASSERT_THAT(result.status, Eq(false));
    ASSERT_THAT(result.frameCount, Eq(0));
}

TEST_F(CommTest, TestGetFrameCount)
{
    EXPECT_CALL(i2c, I2CRead(ReceiverAddress, ReceiverGetFrameCount, Ne(nullptr), 1, Ne(nullptr), 1))
        .WillOnce(Invoke([](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*inLength*/,
            uint8_t* outData,
            uint16_t /*outLength*/) {
            *outData = 31;
            return i2cTransferDone;
        }));

    const auto result = CommGetFrameCount(&comm);
    ASSERT_THAT(result.status, Eq(true));
    ASSERT_THAT(result.frameCount, Eq(31));
}

TEST_F(CommTest, TestClearBeaconFailure)
{
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterClearBeacon, _, 1)).WillOnce(Return(i2cTransferNack));
    const auto status = CommClearBeacon(&comm);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestClearBeacon)
{
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterClearBeacon, Ne(nullptr), 1))
        .WillOnce(Return(i2cTransferDone));
    const auto status = CommClearBeacon(&comm);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestSetIdleStateFailure)
{
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterSetIdleState, _, 2)).WillOnce(Return(i2cTransferNack));
    const auto status = CommSetTransmitterStateWhenIdle(&comm, CommTransmitterOn);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestSetIdleState)
{
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterSetIdleState, Ne(nullptr), 2))
        .WillOnce(Return(i2cTransferDone));
    const auto status = CommSetTransmitterStateWhenIdle(&comm, CommTransmitterOn);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestSetIdleStateCommandOn)
{
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterSetIdleState, Ne(nullptr), 2))
        .WillOnce(Invoke([](uint8_t /*address*/, uint8_t /*command*/, uint8_t* inData, uint16_t /*length*/) {
            EXPECT_THAT(inData[1], Eq(1));
            return i2cTransferDone;
        }));
    const auto status = CommSetTransmitterStateWhenIdle(&comm, CommTransmitterOn);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestSetIdleStateCommandOff)
{
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterSetIdleState, Ne(nullptr), 2))
        .WillOnce(Invoke([](uint8_t /*address*/, uint8_t /*command*/, uint8_t* inData, uint16_t /*length*/) {
            EXPECT_THAT(inData[1], Eq(0));
            return i2cTransferDone;
        }));
    const auto status = CommSetTransmitterStateWhenIdle(&comm, CommTransmitterOff);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestSetBitRateFailure)
{
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterSetBitRate, Ne(nullptr), 2))
        .WillOnce(Return(i2cTransferNack));
    const auto status = CommSetTransmitterBitRate(&comm, Comm1200bps);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestSetBitRate)
{
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterSetBitRate, Ne(nullptr), 2))
        .WillOnce(Return(i2cTransferDone));
    const auto status = CommSetTransmitterBitRate(&comm, Comm1200bps);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestSetBitRateCommand)
{
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterSetBitRate, Ne(nullptr), 2))
        .WillOnce(Invoke([](uint8_t /*address*/, uint8_t /*command*/, uint8_t* inData, uint16_t /*length*/) {
            EXPECT_THAT(inData[1], Eq(1));
            return i2cTransferDone;
        }))
        .WillOnce(Invoke([](uint8_t /*address*/, uint8_t /*command*/, uint8_t* inData, uint16_t /*length*/) {
            EXPECT_THAT(inData[1], Eq(2));
            return i2cTransferDone;
        }))
        .WillOnce(Invoke([](uint8_t /*address*/, uint8_t /*command*/, uint8_t* inData, uint16_t /*length*/) {
            EXPECT_THAT(inData[1], Eq(4));
            return i2cTransferDone;
        }))
        .WillOnce(Invoke([](uint8_t /*address*/, uint8_t /*command*/, uint8_t* inData, uint16_t /*length*/) {
            EXPECT_THAT(inData[1], Eq(8));
            return i2cTransferDone;
        }));
    auto status = CommSetTransmitterBitRate(&comm, Comm1200bps);
    ASSERT_THAT(status, Eq(true));
    status = CommSetTransmitterBitRate(&comm, Comm2400bps);
    ASSERT_THAT(status, Eq(true));
    status = CommSetTransmitterBitRate(&comm, Comm4800bps);
    ASSERT_THAT(status, Eq(true));
    status = CommSetTransmitterBitRate(&comm, Comm9600bps);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestGetTransmitterStateFailure)
{
    CommTransmitterState state;
    EXPECT_CALL(i2c, I2CRead(TransmitterAddress, TransmitterGetState, Ne(nullptr), 1, _, _))
        .WillOnce(Return(i2cTransferNack));
    const auto status = CommGetTransmitterState(&comm, &state);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestGetTransmitterInvalidResponse)
{
    CommTransmitterState state;
    EXPECT_CALL(i2c, I2CRead(TransmitterAddress, TransmitterGetState, Ne(nullptr), 1, Ne(nullptr), 1))
        .WillOnce(Invoke([](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*inLength*/,
            uint8_t* outData,
            uint16_t /*outLength*/) {
            *outData = 0xff;
            return i2cTransferDone;
        }));
    const auto status = CommGetTransmitterState(&comm, &state);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestGetTransmitterResponse)
{
    CommTransmitterState state;
    EXPECT_CALL(i2c, I2CRead(TransmitterAddress, TransmitterGetState, Ne(nullptr), 1, Ne(nullptr), 1))
        .WillOnce(Invoke([](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*inLength*/,
            uint8_t* outData,
            uint16_t /*outLength*/) {
            *outData = 0x7f;
            return i2cTransferDone;
        }));
    const auto status = CommGetTransmitterState(&comm, &state);
    ASSERT_THAT(status, Eq(true));
    ASSERT_THAT(state.BeaconState, Eq(true));
    ASSERT_THAT(state.StateWhenIdle, Eq(CommTransmitterOn));
    ASSERT_THAT(state.TransmitterBitRate, Eq(Comm9600bps));
}

TEST_F(CommTest, TestGetBaseLineTransmitterResponse)
{
    CommTransmitterState state;
    EXPECT_CALL(i2c, I2CRead(TransmitterAddress, TransmitterGetState, Ne(nullptr), 1, Ne(nullptr), 1))
        .WillOnce(Invoke([](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*inLength*/,
            uint8_t* outData,
            uint16_t /*outLength*/) {
            *outData = 0x0;
            return i2cTransferDone;
        }));
    const auto status = CommGetTransmitterState(&comm, &state);
    ASSERT_THAT(status, Eq(true));
    ASSERT_THAT(state.BeaconState, Eq(false));
    ASSERT_THAT(state.StateWhenIdle, Eq(CommTransmitterOff));
    ASSERT_THAT(state.TransmitterBitRate, Eq(Comm1200bps));
}

TEST_F(CommTest, TestGetMixedLineTransmitterResponse)
{
    CommTransmitterState state;
    EXPECT_CALL(i2c, I2CRead(TransmitterAddress, TransmitterGetState, Ne(nullptr), 1, Ne(nullptr), 1))
        .WillOnce(Invoke([](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*inLength*/,
            uint8_t* outData,
            uint16_t /*outLength*/) {
            *outData = 0x0a;
            return i2cTransferDone;
        }));
    const auto status = CommGetTransmitterState(&comm, &state);
    ASSERT_THAT(status, Eq(true));
    ASSERT_THAT(state.BeaconState, Eq(true));
    ASSERT_THAT(state.StateWhenIdle, Eq(CommTransmitterOff));
    ASSERT_THAT(state.TransmitterBitRate, Eq(Comm4800bps));
}

TEST_F(CommTest, TestSendTooLongFrame)
{
    uint8_t buffer[10] = {0};
    const auto status = CommSendFrame(&comm, buffer, COMM_MAX_FRAME_CONTENTS_SIZE + 1);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestSendFrameFailure)
{
    uint8_t buffer[10] = {0};
    EXPECT_CALL(i2c, I2CRead(TransmitterAddress, TransmitterSendFrame, Ne(nullptr), COUNT_OF(buffer) + 1, _, _))
        .WillOnce(Return(i2cTransferNack));
    const auto status = CommSendFrame(&comm, buffer, COUNT_OF(buffer));
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestSendFrame)
{
    uint8_t buffer[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
    EXPECT_CALL(
        i2c, I2CRead(TransmitterAddress, TransmitterSendFrame, Ne(nullptr), COUNT_OF(buffer) + 1, Ne(nullptr), 1))
        .WillOnce(Invoke([](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* inData,
            uint16_t length,
            uint8_t* outData,
            uint16_t /*outLength*/) {
            const uint8_t expected[] = {
                TransmitterSendFrame, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
            EXPECT_THAT(std::equal(inData, inData + length, std::begin(expected), std::end(expected)), Eq(true));
            *outData = 0;
            return i2cTransferDone;
        }));
    const auto status = CommSendFrame(&comm, buffer, COUNT_OF(buffer));
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestSendFrameRejectedByHardware)
{
    uint8_t buffer[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
    EXPECT_CALL(i2c, I2CRead(TransmitterAddress, TransmitterSendFrame, _, _, Ne(nullptr), 1))
        .WillOnce(Invoke([](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*length*/,
            uint8_t* outData,
            uint16_t /*outLength*/) {
            *outData = 0xff;
            return i2cTransferDone;
        }));
    const auto status = CommSendFrame(&comm, buffer, COUNT_OF(buffer));
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestReceiveFrameFailure)
{
    CommFrame frame;
    EXPECT_CALL(i2c, I2CRead(ReceiverAddress, ReceiverGetFrame, _, _, Ne(nullptr), Ge(COMM_MAX_FRAME_CONTENTS_SIZE)))
        .WillOnce(Invoke([](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*length*/,
            uint8_t* /*outData*/,
            uint16_t /*outLength*/) { return i2cTransferNack; }));
    const auto status = CommReceiveFrame(&comm, &frame);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestReceiveFrameDopplerFrequencyOutOfRange)
{
    CommFrame frame;
    EXPECT_CALL(i2c, I2CRead(ReceiverAddress, ReceiverGetFrame, _, _, Ne(nullptr), Ge(COMM_MAX_FRAME_CONTENTS_SIZE)))
        .WillOnce(Invoke([](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*length*/,
            uint8_t* outData,
            uint16_t outLength) {
            memset(outData, 0, outLength);
            outData[0] = 1;
            outData[3] = 0xf0;
            return i2cTransferDone;
        }));
    const auto status = CommReceiveFrame(&comm, &frame);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestReceiveFrameRSSIOutOfRange)
{
    CommFrame frame;
    EXPECT_CALL(i2c, I2CRead(ReceiverAddress, ReceiverGetFrame, _, _, Ne(nullptr), Ge(COMM_MAX_FRAME_CONTENTS_SIZE)))
        .WillOnce(Invoke([](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*length*/,
            uint8_t* outData,
            uint16_t outLength) {
            memset(outData, 0, outLength);
            outData[0] = 1;
            outData[5] = 0xf0;
            return i2cTransferDone;
        }));
    const auto status = CommReceiveFrame(&comm, &frame);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestReceiveFrameSizeOutOfRange)
{
    CommFrame frame;
    EXPECT_CALL(i2c, I2CRead(ReceiverAddress, ReceiverGetFrame, _, _, Ne(nullptr), Ge(COMM_MAX_FRAME_CONTENTS_SIZE)))
        .WillOnce(Invoke([](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*length*/,
            uint8_t* outData,
            uint16_t outLength) {
            memset(outData, 0, outLength);
            outData[0] = 0xff;
            return i2cTransferDone;
        }));
    const auto status = CommReceiveFrame(&comm, &frame);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestReceiveFrameSizeIsZero)
{
    CommFrame frame;
    EXPECT_CALL(i2c, I2CRead(ReceiverAddress, ReceiverGetFrame, _, _, Ne(nullptr), Ge(COMM_MAX_FRAME_CONTENTS_SIZE)))
        .WillOnce(Invoke([](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*length*/,
            uint8_t* outData,
            uint16_t outLength) {
            memset(outData, 0, outLength);
            return i2cTransferDone;
        }));
    const auto status = CommReceiveFrame(&comm, &frame);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestReceiveFrameSizeIsOutOfRange)
{
    CommFrame frame;
    EXPECT_CALL(i2c, I2CRead(ReceiverAddress, ReceiverGetFrame, _, _, Ne(nullptr), Ge(COMM_MAX_FRAME_CONTENTS_SIZE)))
        .WillOnce(Invoke([](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*length*/,
            uint8_t* outData,
            uint16_t outLength) {
            memset(outData, 0, outLength);
            outData[0] = COMM_MAX_FRAME_CONTENTS_SIZE + 1;
            return i2cTransferDone;
        }));
    const auto status = CommReceiveFrame(&comm, &frame);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestReceiveFrame)
{
    CommFrame frame;
    const uint8_t expected[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    EXPECT_CALL(i2c, I2CRead(ReceiverAddress, ReceiverGetFrame, _, _, Ne(nullptr), Ge(COUNT_OF(expected) + 6)))
        .WillOnce(Invoke([&](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*length*/,
            uint8_t* outData,
            uint16_t outLength) {

            memset(outData, 0, outLength);
            outData[0] = COUNT_OF(expected);
            outData[2] = 0xab;
            outData[3] = 0x0c;
            outData[4] = 0xde;
            outData[5] = 0x0d;
            memcpy(outData + 6, expected, COUNT_OF(expected));
            return i2cTransferDone;
        }));
    const auto status = CommReceiveFrame(&comm, &frame);
    ASSERT_THAT(status, Eq(true));
    ASSERT_THAT(frame.Size, Eq(COUNT_OF(expected)));
    ASSERT_THAT(frame.Doppler, Eq(0xcab));
    ASSERT_THAT(frame.RSSI, Eq(0xdde));
    ASSERT_TRUE(std::equal(expected, expected + COUNT_OF(expected), frame.Contents, frame.Contents + frame.Size));
}

TEST_F(CommTest, TestReceiverTelemetry)
{
    CommReceiverTelemetry telemetry;
    const uint8_t expected[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e};
    EXPECT_CALL(
        i2c, I2CRead(ReceiverAddress, ReceverGetTelemetry, _, _, Ne(nullptr), Ge(sizeof(CommReceiverTelemetry))))
        .WillOnce(Invoke([&](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*length*/,
            uint8_t* outData,
            uint16_t outLength) {
            memset(outData, 0, outLength);
            memcpy(outData, expected, COUNT_OF(expected));
            return i2cTransferDone;
        }));
    const auto status = CommGetReceiverTelemetry(&comm, &telemetry);
    ASSERT_THAT(status, Eq(true));
    ASSERT_THAT(telemetry.TransmitterCurrentConsumption, Eq(0x0201));
    ASSERT_THAT(telemetry.DopplerOffset, Eq(0x0403));
    ASSERT_THAT(telemetry.ReceiverCurrentConsumption, Eq(0x0605));
    ASSERT_THAT(telemetry.Vcc, Eq(0x0807));
    ASSERT_THAT(telemetry.OscilatorTemperature, Eq(0x0a09));
    ASSERT_THAT(telemetry.AmplifierTemperature, Eq(0x0c0b));
    ASSERT_THAT(telemetry.SignalStrength, Eq(0x0e0d));
}

TEST_F(CommTest, TestTransmitterTelemetry)
{
    CommTransmitterTelemetry telemetry;
    const uint8_t expected[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    EXPECT_CALL(i2c,
        I2CRead(TransmitterAddress, TransmitterGetTelemetry, _, _, Ne(nullptr), Ge(sizeof(CommTransmitterTelemetry))))
        .WillOnce(Invoke([&](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*length*/,
            uint8_t* outData,
            uint16_t outLength) {
            memset(outData, 0, outLength);
            memcpy(outData, expected, COUNT_OF(expected));
            return i2cTransferDone;
        }));
    const auto status = CommGetTransmitterTelemetry(&comm, &telemetry);
    ASSERT_THAT(status, Eq(true));
    ASSERT_THAT(telemetry.RFReflectedPower, Eq(0x0201));
    ASSERT_THAT(telemetry.AmplifierTemperature, Eq(0x0403));
    ASSERT_THAT(telemetry.RFForwardPower, Eq(0x0605));
    ASSERT_THAT(telemetry.TransmitterCurrentConsumption, Eq(0x0807));
}

TEST_F(CommTest, TestSetBeaconFailure)
{
    CommBeacon beacon;
    memset(&beacon, 0, sizeof(beacon));
    beacon.DataSize = 1;
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterSetBeacon, Ne(nullptr), _))
        .WillOnce(Return(i2cTransferNack));
    const auto status = CommSetBeacon(&comm, &beacon);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestSetBeaconSizeOutOfRange)
{
    CommBeacon beacon;
    memset(&beacon, 0, sizeof(beacon));
    beacon.DataSize = COMM_MAX_FRAME_CONTENTS_SIZE + 1;
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterSetBeacon, Ne(nullptr), _)).Times(0);
    const auto status = CommSetBeacon(&comm, &beacon);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestSetBeacon)
{
    const uint8_t data[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    CommBeacon beacon;
    memset(&beacon, 0, sizeof(beacon));
    beacon.DataSize = COUNT_OF(data);
    beacon.Period = 0x0a0b;
    memcpy(beacon.Data, data, COUNT_OF(data));
    EXPECT_CALL(i2c, I2CWrite(TransmitterAddress, TransmitterSetBeacon, Ne(nullptr), Eq(COUNT_OF(data) + 3)))
        .WillOnce(Invoke([](uint8_t /*address*/, uint8_t /*command*/, uint8_t* inData, uint16_t length) {
            const uint8_t expected[] = {TransmitterSetBeacon, 0x0b, 0x0a, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
            EXPECT_THAT(std::equal(inData, inData + length, std::begin(expected), std::end(expected)), Eq(true));
            return i2cTransferDone;
        }));
    const auto status = CommSetBeacon(&comm, &beacon);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestPauseNonExistingTask)
{
    OSMock system;
    auto guard = InstallProxy(&system);
    EXPECT_CALL(system, SuspendTask(_)).Times(0);
    const auto status = CommPause(&comm);
    ASSERT_THAT(status, Eq(true));
}

class CommReceiverTelemetryTest : public testing::TestWithParam<std::tuple<int, uint8_t, I2C_TransferReturn_TypeDef>>
{
  public:
    CommReceiverTelemetryTest();
    ~CommReceiverTelemetryTest();

  protected:
    CommObject comm;
    I2CMock i2c;
    testing::NiceMock<OSMock> system;
    OSReset reset;
};

CommReceiverTelemetryTest::CommReceiverTelemetryTest()
{
    reset = SetupComm(&comm, system);
    mockPtr = &i2c;
}

CommReceiverTelemetryTest::~CommReceiverTelemetryTest()
{
    mockPtr = nullptr;
}

TEST_P(CommReceiverTelemetryTest, TestInvalidTelemetry)
{
    CommReceiverTelemetry telemetry;
    const auto index = std::get<0>(GetParam());
    const auto value = std::get<1>(GetParam());
    const auto operationStatus = std::get<2>(GetParam());
    EXPECT_CALL(
        i2c, I2CRead(ReceiverAddress, ReceverGetTelemetry, _, _, Ne(nullptr), Ge(sizeof(CommReceiverTelemetry))))
        .WillOnce(Invoke([&](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*length*/,
            uint8_t* outData,
            uint16_t outLength) {
            memset(outData, 0, outLength);
            outData[index] = value;
            return operationStatus;
        }));
    const auto status = CommGetReceiverTelemetry(&comm, &telemetry);
    ASSERT_THAT(status, Eq(false));
}

INSTANTIATE_TEST_CASE_P(CommReceiverTelemetryValuesOutOfRange,
    CommReceiverTelemetryTest,
    testing::Values(std::make_tuple(0, 0, i2cTransferNack),
                            std::make_tuple(1, 0xf0, i2cTransferDone),
                            std::make_tuple(3, 0xf0, i2cTransferDone),
                            std::make_tuple(5, 0xf0, i2cTransferDone),
                            std::make_tuple(7, 0xf0, i2cTransferDone),
                            std::make_tuple(9, 0xf0, i2cTransferDone),
                            std::make_tuple(11, 0xf0, i2cTransferDone),
                            std::make_tuple(13, 0xf0, i2cTransferDone)), );

class CommTransmitterTelemetryTest : public testing::TestWithParam<std::tuple<int, uint8_t, I2C_TransferReturn_TypeDef>>
{
  public:
    CommTransmitterTelemetryTest();
    ~CommTransmitterTelemetryTest();

  protected:
    CommObject comm;
    I2CMock i2c;
    testing::NiceMock<OSMock> system;
    OSReset reset;
};

CommTransmitterTelemetryTest::CommTransmitterTelemetryTest()
{
    reset = SetupComm(&comm, system);
    mockPtr = &i2c;
}

CommTransmitterTelemetryTest::~CommTransmitterTelemetryTest()
{
    mockPtr = nullptr;
}

TEST_P(CommTransmitterTelemetryTest, TestInvalidTelemetry)
{
    CommTransmitterTelemetry telemetry;
    const auto index = std::get<0>(GetParam());
    const auto value = std::get<1>(GetParam());
    const auto operationStatus = std::get<2>(GetParam());
    EXPECT_CALL(i2c,
        I2CRead(TransmitterAddress, TransmitterGetTelemetry, _, _, Ne(nullptr), Ge(sizeof(CommTransmitterTelemetry))))
        .WillOnce(Invoke([&](uint8_t /*address*/,
            uint8_t /*command*/,
            uint8_t* /*inData*/,
            uint16_t /*length*/,
            uint8_t* outData,
            uint16_t outLength) {
            memset(outData, 0, outLength);
            outData[index] = value;
            return operationStatus;
        }));
    const auto status = CommGetTransmitterTelemetry(&comm, &telemetry);
    ASSERT_THAT(status, Eq(false));
}

INSTANTIATE_TEST_CASE_P(CommTransmitterTelemetryValuesOutOfRange,
    CommTransmitterTelemetryTest,
    testing::Values(std::make_tuple(0, 0, i2cTransferNack),
                            std::make_tuple(1, 0xf0, i2cTransferDone),
                            std::make_tuple(3, 0xf0, i2cTransferDone),
                            std::make_tuple(5, 0xf0, i2cTransferDone),
                            std::make_tuple(7, 0xf0, i2cTransferDone)), );