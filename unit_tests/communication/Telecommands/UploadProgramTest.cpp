#include <algorithm>
#include <array>
#include <cstdint>
#include <gsl/span>
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "mock/comm.hpp"
#include "obc/telecommands/program_upload.hpp"
#include "program_flash/boot_table.hpp"
#include "utils.h"

using testing::_;
using testing::A;
using testing::Invoke;
using testing::Eq;
using testing::Return;
using testing::StrEq;
using testing::Each;
using testing::ElementsAre;
using telecommunication::downlink::DownlinkAPID;
using program_flash::FlashStatus;

struct FlashDriverMock : public program_flash::IFlashDriver
{
    MOCK_CONST_METHOD1(At, std::uint8_t const*(std::size_t offset));
    MOCK_METHOD2(Program, FlashStatus(std::size_t offset, std::uint8_t value));
    MOCK_METHOD2(Program, FlashStatus(std::size_t offset, gsl::span<const std::uint8_t> value));
    MOCK_METHOD1(EraseSector, FlashStatus(std::size_t sectorOfffset));

    MOCK_CONST_METHOD0(DeviceId, std::uint32_t());
    MOCK_CONST_METHOD0(BootConfig, std::uint32_t());
};

static std::array<std::uint8_t, 4_MB> Flash;

class UploadProgramTest : public testing::Test
{
  public:
    UploadProgramTest();

  protected:
    gsl::span<std::uint8_t> _flash;
    testing::NiceMock<FlashDriverMock> _flashMock;
    program_flash::BootTable _bootTable;

    testing::NiceMock<TransmitterMock> _transmitter;

    obc::telecommands::EraseBootTableEntry _eraseTelecommand;
    obc::telecommands::WriteProgramPart _writePartTelecommand;
    obc::telecommands::FinalizeProgramEntry _finalizeTelecommand;

    template <typename... Values> void HandleFrame(telecommunication::uplink::IHandleTeleCommand& telecommand, Values... parameters)
    {
        std::array<std::uint8_t, sizeof...(Values)> params{static_cast<std::uint8_t>(parameters)...};

        telecommand.Handle(this->_transmitter, params);
    }
};

UploadProgramTest::UploadProgramTest()
    : _flash(Flash), _bootTable(_flashMock), _eraseTelecommand(_bootTable), _writePartTelecommand(_bootTable),
      _finalizeTelecommand(_bootTable)
{
    Flash.fill(0xA5);

    ON_CALL(this->_flashMock, DeviceId()).WillByDefault(Return(0x00530000));
    ON_CALL(this->_flashMock, BootConfig()).WillByDefault(Return(0x00000002));

    ON_CALL(this->_flashMock, At(_)).WillByDefault(Invoke([this](std::size_t offset) { return this->_flash.data() + offset; }));

    ON_CALL(this->_flashMock, EraseSector(_)).WillByDefault(Invoke([this](std::size_t sectorOffset) {
        auto sector = this->_flash.subspan(sectorOffset, 512_KB);
        std::fill(sector.begin(), sector.end(), 0xFF);
        return FlashStatus::NotBusy;
    }));

    ON_CALL(this->_flashMock, Program(_, A<gsl::span<const std::uint8_t>>()))
        .WillByDefault(Invoke([this](std::size_t offset, gsl::span<const std::uint8_t> value) {
            std::copy(value.begin(), value.end(), this->_flash.begin() + offset);

            return FlashStatus::NotBusy;
        }));
    ON_CALL(this->_flashMock, Program(_, A<std::uint8_t>())).WillByDefault(Invoke([this](std::size_t offset, std::uint8_t value) {
        this->_flash[offset] = value;
        return FlashStatus::NotBusy;
    }));

    this->_bootTable.Initialize();
}

TEST_F(UploadProgramTest, ReadProgramDetails)
{
    this->_flash[0x00080000] = 0x12;
    this->_flash[0x00080001] = 0x13;
    this->_flash[0x00080002] = 0;
    this->_flash[0x00080003] = 0;

    this->_flash[0x00080020] = 0xAB;
    this->_flash[0x00080021] = 0xCD;

    this->_flash[0x00080040] = 0xAA;

    strcpy(reinterpret_cast<char*>(&this->_flash[0x00080080]), "Test");
    strcpy(reinterpret_cast<char*>(&this->_flash[0x00080400]), "Program");

    auto entry = this->_bootTable.Entry(1);

    ASSERT_THAT(entry.Length(), Eq(0x1312U));
    ASSERT_THAT(entry.Crc(), Eq(0xCDAB));
    ASSERT_THAT(entry.IsValid(), Eq(true));
    ASSERT_THAT(entry.Description(), StrEq("Test"));
    ASSERT_THAT(reinterpret_cast<const char*>(entry.Content()), StrEq("Program"));
}

TEST_F(UploadProgramTest, ReadProgramCRC)
{
    auto entry = this->_bootTable.Entry(1);
    entry.Length(0x1312);

    auto crc = entry.CalculateCrc();
    ASSERT_THAT(crc, Eq(0xD5C7));
}

TEST_F(UploadProgramTest, ProgramSequence)
{
    auto entry = this->_bootTable.Entry(1);

    entry.Erase();

    entry.Length(0x1312U);
    entry.Crc(0xCDAB);
    entry.Description("Test");
    entry.WriteContent(0, gsl::make_span(reinterpret_cast<const uint8_t*>("Program"), 8));
    entry.MarkAsValid();

    ASSERT_THAT(entry.Length(), Eq(0x1312U));
    ASSERT_THAT(entry.Crc(), Eq(0xCDAB));
    ASSERT_THAT(entry.IsValid(), Eq(true));
    ASSERT_THAT(entry.Description(), StrEq("Test"));
    ASSERT_THAT(reinterpret_cast<const char*>(entry.Content()), StrEq("Program"));
}

TEST_F(UploadProgramTest, WriteProgramByTelecommands)
{
    EXPECT_CALL(this->_transmitter, SendFrame(IsDownlinkFrame(DownlinkAPID::ProgramUpload, 0U, ElementsAre(0, 0, 1)))).Times(1);

    this->HandleFrame(this->_eraseTelecommand, 1);

    EXPECT_CALL(this->_transmitter, SendFrame(IsDownlinkFrame(DownlinkAPID::ProgramUpload, 0U, ElementsAre(1, 0, 1, 0, 4, 0, 0, 5))))
        .Times(1);
    this->HandleFrame(this->_writePartTelecommand, 1, 0x00, 0x04, 0x00, 0x00, 'P', 'a', 'r', 't', 0);

    EXPECT_CALL(this->_transmitter, SendFrame(IsDownlinkFrame(DownlinkAPID::ProgramUpload, 0U, ElementsAre(1, 0, 1, 0, 0, 0, 0, 8))))
        .Times(1);
    this->HandleFrame(this->_writePartTelecommand, 1, 0x00, 0x00, 0x00, 0x00, 'P', 'r', 'o', 'g', 'r', 'a', 'm', 0);

    EXPECT_CALL(this->_transmitter, SendFrame(IsDownlinkFrame(DownlinkAPID::ProgramUpload, 0U, ElementsAre(2, 0, 1, 0x84, 0xD8))));
    this->HandleFrame(this->_finalizeTelecommand, 1, 0x12, 0x13, 0x00, 0x00, 0x84, 0xD8, 'T', 'e', 's', 't');

    auto entry = this->_bootTable.Entry(1);
    ASSERT_THAT(entry.Length(), Eq(0x1312U));
    ASSERT_THAT(entry.Crc(), Eq(0xD884));
    ASSERT_THAT(entry.IsValid(), Eq(true));
    ASSERT_THAT(entry.Description(), StrEq("Test"));
    ASSERT_THAT(reinterpret_cast<const char*>(entry.Content()), StrEq("Program"));
    ASSERT_THAT(reinterpret_cast<const char*>(entry.Content() + 1_KB), StrEq("Part"));
}

TEST_F(UploadProgramTest, ResponseWithEraseError)
{
    ON_CALL(this->_flashMock, EraseSector(0x00080000 + 5 * 64_KB)).WillByDefault(Return(FlashStatus::EraseError));

    EXPECT_CALL(
        this->_transmitter, SendFrame(IsDownlinkFrame(DownlinkAPID::ProgramUpload, 0U, ElementsAre(0, 1, 9, 1, 0x00, 0x0, 0x05, 0x00))))
        .Times(1);

    this->HandleFrame(this->_eraseTelecommand, 1);
}

TEST_F(UploadProgramTest, ResponseWithProgramErrorOnWritePart)
{
    ON_CALL(this->_flashMock, Program(0x00080400 + 256_KB, A<gsl::span<const std::uint8_t>>()))
        .WillByDefault(Return(FlashStatus::ProgramError));

    EXPECT_CALL(
        this->_transmitter, SendFrame(IsDownlinkFrame(DownlinkAPID::ProgramUpload, 0U, ElementsAre(1, 1, 10, 1, 0x00, 0x00, 0x04, 0x00))))
        .Times(1);

    this->HandleFrame(this->_writePartTelecommand, 1, 0x00, 0x00, 0x04, 0x00, 'P', 'a', 'r', 't', 0);
}

TEST_F(UploadProgramTest, ResponseWithProgramErrorOnFinalize)
{
    ON_CALL(this->_flashMock, Program(0x00080000, A<gsl::span<const std::uint8_t>>())).WillByDefault(Return(FlashStatus::ProgramError));

    EXPECT_CALL(this->_transmitter, SendFrame(IsDownlinkFrame(DownlinkAPID::ProgramUpload, 0U, ElementsAre(2, 1, 10, 1)))).Times(1);

    this->HandleFrame(this->_finalizeTelecommand, 1, 0x12, 0x13, 0x14, 0x15, 0xAB, 0xCD, 'T', 'e', 's', 't');
}

TEST_F(UploadProgramTest, ResponseWithErrorOnFinalizeWithIncorrectCRC)
{
    EXPECT_CALL(this->_transmitter, SendFrame(IsDownlinkFrame(DownlinkAPID::ProgramUpload, 0U, ElementsAre(2, 20, 1, 0xC7, 0xD5))))
        .Times(1);

    this->HandleFrame(this->_finalizeTelecommand, 1, 0x12, 0x13, 0x00, 0x00, 0xAB, 0xCD, 'T', 'e', 's', 't');
}
