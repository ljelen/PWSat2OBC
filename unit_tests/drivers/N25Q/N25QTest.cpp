#include <algorithm>
#include <array>
#include <limits>

#include <gsl/span>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "n25q/n25q.h"
#include "spi/spi.h"
#include "utils.hpp"

#include "OsMock.hpp"
#include "SPI/SPIMock.h"
#include "base/os.h"
#include "mock/error_counter.hpp"
#include "os/os.hpp"

using std::array;
using std::copy;
using gsl::span;

using testing::Test;
using testing::ContainerEq;
using testing::Eq;
using testing::NiceMock;
using testing::StrictMock;
using testing::_;
using testing::Invoke;
using testing::ElementsAre;
using testing::PrintToString;
using testing::InSequence;
using testing::WithArg;
using testing::Return;
using testing::AtLeast;
using testing::DoAll;

using drivers::spi::ISPIInterface;
using namespace devices::n25q;
using namespace std::chrono_literals;

enum Command
{
    ReadId = 0x9E,
    ReadStatusRegister = 0x05,
    ReadFlagStatusRegister = 0x70,
    ReadMemory = 0x03,
    WriteEnable = 0x06,
    WriteDisable = 0x05,
    ProgramMemory = 0x02,
    EraseSubsector = 0x20,
    EraseSector = 0xD8,
    EraseChip = 0xC7,
    ClearFlagRegister = 0x50,
    ResetEnable = 0x66,
    ResetMemory = 0x99,
    WriteStatusRegister = 0x01
};

MATCHER_P(CommandCall, command, std::string("Command " + PrintToString(command)))
{
    return arg.size() == 1 && arg[0] == command;
}

static Status operator|(Status lhs, Status rhs)
{
    using U = std::underlying_type_t<Status>;

    return static_cast<Status>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

static FlagStatus operator|(FlagStatus lhs, FlagStatus rhs)
{
    using U = std::underlying_type_t<FlagStatus>;

    return static_cast<FlagStatus>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

class N25QDriverTest : public Test
{
  public:
    N25QDriverTest();
    virtual ~N25QDriverTest();

  protected:
    decltype(auto) ExpectCommand(Command command)
    {
        return EXPECT_CALL(this->_spi, Write(CommandCall(command)));
    }

    void ExpectCommandAndRespondOnce(Command command, span<uint8_t> response)
    {
        EXPECT_CALL(this->_spi, Write(CommandCall(command)));
        EXPECT_CALL(this->_spi, Read(SpanOfSize(response.size()))).WillOnce(DoAll(FillBuffer<0>(response), Return(OSResult::Success)));
    }

    void ExpectCommandAndRespondOnce(Command command, uint8_t response)
    {
        EXPECT_CALL(this->_spi, Write(CommandCall(command)));
        EXPECT_CALL(this->_spi, Read(SpanOfSize(1))).WillOnce(DoAll(FillBuffer<0>(response), Return(OSResult::Success)));
    }

    void ExpectCommandAndRespondOnce(Command command, Status response)
    {
        EXPECT_CALL(this->_spi, Write(CommandCall(command)));
        EXPECT_CALL(this->_spi, Read(SpanOfSize(1))).WillOnce(DoAll(FillBuffer<0>(num(response)), Return(OSResult::Success)));
    }

    void ExpectCommandAndRespondOnce(Command command, FlagStatus response)
    {
        EXPECT_CALL(this->_spi, Write(CommandCall(command)));
        EXPECT_CALL(this->_spi, Read(SpanOfSize(1))).WillOnce(DoAll(FillBuffer<0>(num(response)), Return(OSResult::Success)));
    }

    void ExpectCommandAndTimeoutOnce(Command command)
    {
        EXPECT_CALL(this->_spi, Write(CommandCall(command)));
        EXPECT_CALL(this->_spi, Read(testing::_)).WillOnce(Return(OSResult::Timeout));
    }

    void ExpectCommandAndRespondManyTimes(Command command, uint8_t response, uint16_t times)
    {
        for (auto i = 0; i < times; i++)
        {
            EXPECT_CALL(this->_spi, Write(CommandCall(command)));
            EXPECT_CALL(this->_spi, Read(SpanOfSize(1))).WillOnce(DoAll(FillBuffer<0>(response), Return(OSResult::Success)));
        }
    }

    void ExpectWaitBusy(uint16_t busyCycles)
    {
        auto selected = this->_spi.ExpectSelected();

        ExpectCommandAndRespondManyTimes(Command::ReadStatusRegister, num(Status::WriteEnabled | Status::WriteInProgress), busyCycles);

        ExpectCommandAndRespondOnce(Command::ReadStatusRegister, Status::WriteDisabled);
    }

    void ExpectClearFlags()
    {
        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::ClearFlagRegister);
        }
    }

    testing::NiceMock<ErrorCountingConfigrationMock> _errorsConfig;
    error_counter::ErrorCounting _errors;
    error_counter::ErrorCounter<1> _error_counter;

    StrictMock<SPIInterfaceMock> _spi;
    N25QDriver _driver;
    NiceMock<OSMock> _os;
    OSReset _osReset;

    std::array<uint8_t, 3> _incorrectId;
    std::array<uint8_t, 3> _correctId;
};

N25QDriverTest::N25QDriverTest()
    :                                 //
      _errors{_errorsConfig},         //
      _error_counter{_errors},        //
      _driver{_errors, 1, _spi},      //
      _incorrectId{0xAA, 0xBB, 0xCC}, //
      _correctId{0x20, 0xBA, 0x18}    //
{
    this->_osReset = InstallProxy(&this->_os);

    ON_CALL(this->_os, GetUptime()).WillByDefault(Return(0ms));

    testing::DefaultValue<OSResult>::Set(OSResult::Success);
}

N25QDriverTest::~N25QDriverTest()
{
    testing::DefaultValue<OSResult>::Clear();
}

TEST_F(N25QDriverTest, ShouldReadIdCorrectly)
{
    {
        InSequence s;

        auto selected = this->_spi.ExpectSelected();

        ExpectCommandAndRespondOnce(Command::ReadId, _correctId);
    }

    auto id = this->_driver.ReadId();

    ASSERT_THAT(id.Manufacturer, Eq(0x20));
    ASSERT_THAT(id.MemoryType, Eq(0xBA));
    ASSERT_THAT(id.MemoryCapacity, Eq(0x18));
    ASSERT_THAT(_error_counter, Eq(0));
}

TEST_F(N25QDriverTest, ShouldRetryReadIdOnTimeout)
{
    {
        InSequence s;

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndTimeoutOnce(Command::ReadId);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadId, _correctId);
        }
    }

    auto id = this->_driver.ReadId();

    ASSERT_THAT(id.Manufacturer, Eq(0x20));
    ASSERT_THAT(id.MemoryType, Eq(0xBA));
    ASSERT_THAT(id.MemoryCapacity, Eq(0x18));
    ASSERT_THAT(_error_counter, Eq(0));
}

TEST_F(N25QDriverTest, ShouldReadStatusRegisterCorrectly)
{
    {
        InSequence s;

        auto selected = this->_spi.ExpectSelected();

        ExpectCommandAndRespondOnce(Command::ReadStatusRegister, Status::WriteDisabled | Status::WriteInProgress);
    }

    auto status = this->_driver.ReadStatus();

    ASSERT_THAT(status, Eq(Status::WriteDisabled | Status::WriteInProgress));
    ASSERT_THAT(_error_counter, Eq(0));
}

TEST_F(N25QDriverTest, ShouldRetryReadStatusRegisterCorrectlyOnTimeout)
{
    {
        InSequence s;

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndTimeoutOnce(Command::ReadStatusRegister);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadStatusRegister, Status::WriteDisabled | Status::WriteInProgress);
        }
    }

    auto status = this->_driver.ReadStatus();

    ASSERT_THAT(status, Eq(Status::WriteDisabled | Status::WriteInProgress));
    ASSERT_THAT(_error_counter, Eq(0));
}

TEST_F(N25QDriverTest, ShouldReadFlagStatusRegisterCorrectly)
{
    {
        InSequence s;

        auto selected = this->_spi.ExpectSelected();

        ExpectCommandAndRespondOnce(Command::ReadFlagStatusRegister, FlagStatus::EraseSuspended | FlagStatus::ProgramError);
    }

    auto status = this->_driver.ReadFlagStatus();

    ASSERT_THAT(status, Eq(FlagStatus::EraseSuspended | FlagStatus::ProgramError));
    ASSERT_THAT(_error_counter, Eq(0));
}

TEST_F(N25QDriverTest, ShouldRetryReadFlagStatusRegisterOnTimeout)
{
    {
        InSequence s;

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndTimeoutOnce(Command::ReadFlagStatusRegister);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadFlagStatusRegister, FlagStatus::EraseSuspended | FlagStatus::ProgramError);
        }
    }

    auto status = this->_driver.ReadFlagStatus();

    ASSERT_THAT(status, Eq(FlagStatus::EraseSuspended | FlagStatus::ProgramError));
    ASSERT_THAT(_error_counter, Eq(0));
}

TEST_F(N25QDriverTest, ReadRequestShouldBePropertlyFormed)
{
    const uint32_t address = 0xAB0000;

    array<uint8_t, 260> memory;
    memory.fill(0xCC);

    {
        InSequence s;
        auto selected = this->_spi.ExpectSelected();

        EXPECT_CALL(this->_spi, Write(ElementsAre(Command::ReadMemory, 0xAB, 0x00, 0x00)));

        EXPECT_CALL(this->_spi, Read(SpanOfSize(260))).WillOnce(DoAll(FillBuffer<0>(memory), Return(OSResult::Success)));
    }

    array<uint8_t, 260> buffer;
    buffer.fill(0);

    this->_driver.ReadMemory(address, buffer);

    ASSERT_THAT(buffer, ContainerEq(memory));
    ASSERT_THAT(_error_counter, Eq(0));
}

TEST_F(N25QDriverTest, ShouldRetryReadOnTimeout)
{
    const uint32_t address = 0xAB0000;

    array<uint8_t, 260> memory;
    memory.fill(0xCC);

    {
        InSequence s;

        {
            auto selected = this->_spi.ExpectSelected();

            EXPECT_CALL(this->_spi, Write(ElementsAre(Command::ReadMemory, 0xAB, 0x00, 0x00))).WillOnce(Return(OSResult::Timeout));
        }

        {
            auto selected = this->_spi.ExpectSelected();

            EXPECT_CALL(this->_spi, Write(ElementsAre(Command::ReadMemory, 0xAB, 0x00, 0x00)));

            EXPECT_CALL(this->_spi, Read(SpanOfSize(260))).WillOnce(Return(OSResult::Timeout));
        }

        {
            auto selected = this->_spi.ExpectSelected();

            EXPECT_CALL(this->_spi, Write(ElementsAre(Command::ReadMemory, 0xAB, 0x00, 0x00)));

            EXPECT_CALL(this->_spi, Read(SpanOfSize(260))).WillOnce(DoAll(FillBuffer<0>(memory), Return(OSResult::Success)));
        }
    }

    array<uint8_t, 260> buffer;
    buffer.fill(0);

    this->_driver.ReadMemory(address, buffer);

    ASSERT_THAT(buffer, ContainerEq(memory));
    ASSERT_THAT(_error_counter, Eq(0));
}

TEST_F(N25QDriverTest, ShouldReturnErrorWhenAllRetriesFailed)
{
    const uint32_t address = 0xAB0000;

    array<uint8_t, 260> memory;
    memory.fill(0xCC);

    {
        InSequence s;

        {
            auto selected = this->_spi.ExpectSelected();

            EXPECT_CALL(this->_spi, Write(ElementsAre(Command::ReadMemory, 0xAB, 0x00, 0x00))).WillOnce(Return(OSResult::Timeout));
        }

        {
            auto selected = this->_spi.ExpectSelected();

            EXPECT_CALL(this->_spi, Write(ElementsAre(Command::ReadMemory, 0xAB, 0x00, 0x00))).WillOnce(Return(OSResult::Timeout));
        }

        {
            auto selected = this->_spi.ExpectSelected();

            EXPECT_CALL(this->_spi, Write(ElementsAre(Command::ReadMemory, 0xAB, 0x00, 0x00))).WillOnce(Return(OSResult::Timeout));
        }
    }

    array<uint8_t, 260> buffer;
    buffer.fill(0);

    auto r = this->_driver.ReadMemory(address, buffer);

    ASSERT_THAT(r, Eq(OSResult::Timeout));
    ASSERT_THAT(_error_counter, Eq(5));
}

TEST_F(N25QDriverTest, ShouldWriteSinglePage)
{
    const uint32_t address = 0xAB0000;

    array<uint8_t, 256> buffer;
    buffer.fill(0xCC);

    {
        InSequence s;

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::WriteEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            EXPECT_CALL(this->_spi, Write(ElementsAre(Command::ProgramMemory, 0xAB, 0x00, 0x00)));

            EXPECT_CALL(this->_spi, Write(ContainerEq(span<const uint8_t>(buffer))));
        }

        ExpectWaitBusy(2);

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadFlagStatusRegister, FlagStatus::Clear);
        }
    }

    auto result = this->_driver.BeginWritePage(address, 0, buffer).Wait();
    ASSERT_THAT(result, Eq(OperationResult::Success));
    ASSERT_THAT(_error_counter, Eq(0));
}

TEST_F(N25QDriverTest, ShouldDetectProgramErrors)
{
    const uint32_t address = 0xAB0000;

    array<uint8_t, 256> buffer;
    buffer.fill(0xCC);

    {
        InSequence s;

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::WriteEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            EXPECT_CALL(this->_spi, Write(ElementsAre(Command::ProgramMemory, 0xAB, 0x00, 0x00)));

            EXPECT_CALL(this->_spi, Write(ContainerEq(span<const uint8_t>(buffer).subspan(0, 256))));
        }

        ExpectWaitBusy(4);

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadFlagStatusRegister, FlagStatus::ProgramError);
        }
    }

    auto result = this->_driver.BeginWritePage(address, 0, buffer).Wait();

    ASSERT_THAT(result, Eq(OperationResult::Failure));
    ASSERT_THAT(_error_counter, Eq(5));
}

TEST_F(N25QDriverTest, ShouldEraseSubsector)
{
    const uint32_t address = 0xCD0000;

    {
        InSequence s;

        ExpectClearFlags();

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::WriteEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::EraseSubsector);

            EXPECT_CALL(this->_spi, Write(ElementsAre(0xCD, 0x00, 0x00)));
        }

        ExpectWaitBusy(4);

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadFlagStatusRegister, FlagStatus::Clear);
        }
    }

    auto result = this->_driver.BeginEraseSubSector(address).Wait();

    ASSERT_THAT(result, Eq(OperationResult::Success));
    ASSERT_THAT(_error_counter, Eq(0));
}

TEST_F(N25QDriverTest, ShouldDetectEraseSubsectorError)
{
    const uint32_t address = 0xCD0000;

    {
        InSequence s;

        ExpectClearFlags();

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::WriteEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::EraseSubsector);

            EXPECT_CALL(this->_spi, Write(ElementsAre(0xCD, 0x00, 0x00)));
        }

        ExpectWaitBusy(4);

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadFlagStatusRegister, FlagStatus::EraseError);
        }
    }

    auto result = this->_driver.BeginEraseSubSector(address).Wait();

    ASSERT_THAT(result, Eq(OperationResult::Failure));
    ASSERT_THAT(_error_counter, Eq(5));
}

TEST_F(N25QDriverTest, ShouldDetectEraseSubsectorTimeout)
{
    const uint32_t address = 0xCD0000;

    {
        InSequence s;

        ExpectClearFlags();

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::WriteEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::EraseSubsector);

            EXPECT_CALL(this->_spi, Write(ElementsAre(0xCD, 0x00, 0x00)));
        }

        {
            auto selected = this->_spi.ExpectSelected();

            EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(0ms));

            ExpectCommandAndRespondOnce(Command::ReadStatusRegister, Status::WriteEnabled | Status::WriteInProgress);

            EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(0ms));

            ExpectCommandAndRespondOnce(Command::ReadStatusRegister, Status::WriteEnabled | Status::WriteInProgress);

            EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(std::chrono::milliseconds(std::numeric_limits<uint32_t>::max())));
        }
    }

    auto result = this->_driver.BeginEraseSubSector(address).Wait();

    ASSERT_THAT(result, Eq(OperationResult::Timeout));
    ASSERT_THAT(_error_counter, Eq(5));
}

TEST_F(N25QDriverTest, ShouldEraseSector)
{
    const uint32_t address = 0xCD0000;

    {
        InSequence s;

        ExpectClearFlags();

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::WriteEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::EraseSector);

            EXPECT_CALL(this->_spi, Write(ElementsAre(0xCD, 0x00, 0x00)));
        }

        ExpectWaitBusy(4);

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadFlagStatusRegister, FlagStatus::Clear);
        }
    }

    auto result = this->_driver.BeginEraseSector(address).Wait();

    ASSERT_THAT(result, Eq(OperationResult::Success));
    ASSERT_THAT(_error_counter, Eq(0));
}

TEST_F(N25QDriverTest, ShouldDetectEraseSectorError)
{
    const uint32_t address = 0xCD0000;

    {
        InSequence s;

        ExpectClearFlags();

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::WriteEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::EraseSector);

            EXPECT_CALL(this->_spi, Write(ElementsAre(0xCD, 0x00, 0x00)));
        }

        ExpectWaitBusy(4);

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadFlagStatusRegister, FlagStatus::EraseError);
        }
    }

    auto result = this->_driver.BeginEraseSector(address).Wait();

    ASSERT_THAT(result, Eq(OperationResult::Failure));
    ASSERT_THAT(_error_counter, Eq(5));
}

TEST_F(N25QDriverTest, ShouldDetectEraseSectorTimeout)
{
    const uint32_t address = 0xCD0000;

    {
        InSequence s;

        ExpectClearFlags();

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::WriteEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::EraseSector);

            EXPECT_CALL(this->_spi, Write(ElementsAre(0xCD, 0x00, 0x00)));
        }

        {
            auto selected = this->_spi.ExpectSelected();

            EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(0ms));

            ExpectCommandAndRespondOnce(Command::ReadStatusRegister, Status::WriteEnabled | Status::WriteInProgress);

            EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(0ms));

            ExpectCommandAndRespondOnce(Command::ReadStatusRegister, Status::WriteEnabled | Status::WriteInProgress);

            EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(std::chrono::milliseconds(std::numeric_limits<uint32_t>::max())));
        }
    }

    auto result = this->_driver.BeginEraseSector(address).Wait();

    ASSERT_THAT(result, Eq(OperationResult::Timeout));
    ASSERT_THAT(_error_counter, Eq(5));
}

TEST_F(N25QDriverTest, ShouldEraseChip)
{
    {
        InSequence s;

        ExpectClearFlags();

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::WriteEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::EraseChip);
        }

        ExpectWaitBusy(4);

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadFlagStatusRegister, FlagStatus::Clear);
        }
    }

    auto result = this->_driver.BeginEraseChip().Wait();

    ASSERT_THAT(result, Eq(OperationResult::Success));
    ASSERT_THAT(_error_counter, Eq(0));
}

TEST_F(N25QDriverTest, ShouldDetectEraseChipError)
{
    {
        InSequence s;

        ExpectClearFlags();

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::WriteEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::EraseChip);
        }

        ExpectWaitBusy(4);

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadFlagStatusRegister, FlagStatus::EraseError);
        }
    }

    auto result = this->_driver.BeginEraseChip().Wait();

    ASSERT_THAT(result, Eq(OperationResult::Failure));
    ASSERT_THAT(_error_counter, Eq(5));
}

TEST_F(N25QDriverTest, EraseChipOperationWillTimeout)
{
    {
        InSequence s;

        ExpectClearFlags();

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::WriteEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::EraseChip);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(0ms));

            ExpectCommandAndRespondOnce(Command::ReadStatusRegister, Status::WriteEnabled | Status::WriteInProgress);

            EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(0ms));

            ExpectCommandAndRespondOnce(Command::ReadStatusRegister, Status::WriteEnabled | Status::WriteInProgress);

            EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(std::chrono::milliseconds(std::numeric_limits<uint32_t>::max())));
        }
    }

    auto result = this->_driver.BeginEraseChip().Wait();

    ASSERT_THAT(result, Eq(OperationResult::Timeout));

    ASSERT_THAT(_error_counter, Eq(5));
}

TEST_F(N25QDriverTest, ShouldResetProperly)
{
    {
        InSequence s;

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::ResetEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::ResetMemory);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadId, _incorrectId);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadId, _correctId);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::WriteEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::WriteStatusRegister);
            EXPECT_CALL(this->_spi, Write(ElementsAre(0x20)));
        }

        ExpectWaitBusy(3);
    }

    auto result = this->_driver.Reset();

    ASSERT_THAT(result, Eq(OperationResult::Success));
    ASSERT_THAT(_error_counter, Eq(0));
}

TEST_F(N25QDriverTest, SettingProtectionOnResetCanTimeout)
{
    {
        InSequence s;

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::ResetEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::ResetMemory);
        }

        EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(0ms));

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadId, _correctId);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::WriteEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::WriteStatusRegister);
            EXPECT_CALL(this->_spi, Write(ElementsAre(0x20)));
        }

        {
            auto selected = this->_spi.ExpectSelected();

            EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(0ms));

            ExpectCommandAndRespondOnce(Command::ReadStatusRegister, Status::WriteEnabled | Status::WriteInProgress);

            EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(0ms));

            ExpectCommandAndRespondOnce(Command::ReadStatusRegister, Status::WriteEnabled | Status::WriteInProgress);

            EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(std::chrono::milliseconds(std::numeric_limits<uint32_t>::max())));
        }
    }

    auto result = this->_driver.Reset();

    ASSERT_THAT(result, Eq(OperationResult::Timeout));
    ASSERT_THAT(_error_counter, Eq(5));
}

TEST_F(N25QDriverTest, WaitingOnResetCanTimeout)
{
    {
        InSequence s;

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::ResetEnable);
        }

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommand(Command::ResetMemory);
        }

        EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(0ms));
        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadId, _incorrectId);
        }
        EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(0ms));

        {
            auto selected = this->_spi.ExpectSelected();

            ExpectCommandAndRespondOnce(Command::ReadId, _incorrectId);
        }
        EXPECT_CALL(this->_os, GetUptime()).WillRepeatedly(Return(std::chrono::milliseconds(std::numeric_limits<uint32_t>::max())));
    }

    auto result = this->_driver.Reset();

    ASSERT_THAT(result, Eq(OperationResult::Timeout));
    ASSERT_THAT(_error_counter, Eq(5));
}
