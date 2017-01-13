#include <algorithm>
#include <em_i2c.h>
#include <gsl/span>
#include <string>
#include <tuple>
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "gmock/gmock-matchers.h"
#include "OsMock.hpp"
#include "i2c/I2CMock.hpp"
#include "i2c/i2c.h"
#include "os/os.hpp"
#include "system.h"
#include "utils.hpp"

#include "imtq/imtq.h"


using testing::_;
using testing::Eq;
using testing::Ne;
using testing::Ge;
using testing::StrEq;
using testing::Return;
using testing::Invoke;
using testing::Pointee;
using testing::ElementsAre;
using testing::Matches;
using gsl::span;
using drivers::i2c::I2CResult;

static const uint8_t ImtqAddress = 0x10;

TEST(ImtqTestDataStructures, Status)
{
	devices::imtq::Status status{0x00};
	EXPECT_EQ(status.IsNew(), false);
	EXPECT_EQ(status.InvalidX(), false);
	EXPECT_EQ(status.InvalidY(), false);
	EXPECT_EQ(status.InvalidZ(), false);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::Accepted);

	status = devices::imtq::Status{0x11};
	EXPECT_EQ(status.IsNew(), false);
	EXPECT_EQ(status.InvalidX(), false);
	EXPECT_EQ(status.InvalidY(), false);
	EXPECT_EQ(status.InvalidZ(), true);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::Rejected);

	status = devices::imtq::Status{0x22};
	EXPECT_EQ(status.IsNew(), false);
	EXPECT_EQ(status.InvalidX(), false);
	EXPECT_EQ(status.InvalidY(), true);
	EXPECT_EQ(status.InvalidZ(), false);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::InvalidCommandCode);

	status = devices::imtq::Status{0x33};
	EXPECT_EQ(status.IsNew(), false);
	EXPECT_EQ(status.InvalidX(), false);
	EXPECT_EQ(status.InvalidY(), true);
	EXPECT_EQ(status.InvalidZ(), true);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::ParameterMissing);

	status = devices::imtq::Status{0x44};
	EXPECT_EQ(status.IsNew(), false);
	EXPECT_EQ(status.InvalidX(), true);
	EXPECT_EQ(status.InvalidY(), false);
	EXPECT_EQ(status.InvalidZ(), false);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::ParameterInvalid);

	status = devices::imtq::Status{0x55};
	EXPECT_EQ(status.IsNew(), false);
	EXPECT_EQ(status.InvalidX(), true);
	EXPECT_EQ(status.InvalidY(), false);
	EXPECT_EQ(status.InvalidZ(), true);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::CommandUnavailableInCurrentMode);

	status = devices::imtq::Status{0x67};
	EXPECT_EQ(status.IsNew(), false);
	EXPECT_EQ(status.InvalidX(), true);
	EXPECT_EQ(status.InvalidY(), true);
	EXPECT_EQ(status.InvalidZ(), false);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::InternalError);

	status = devices::imtq::Status{0x70};
	EXPECT_EQ(status.IsNew(), false);
	EXPECT_EQ(status.InvalidX(), true);
	EXPECT_EQ(status.InvalidY(), true);
	EXPECT_EQ(status.InvalidZ(), true);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::Accepted);

	status = devices::imtq::Status{0x81};
	EXPECT_EQ(status.IsNew(), true);
	EXPECT_EQ(status.InvalidX(), false);
	EXPECT_EQ(status.InvalidY(), false);
	EXPECT_EQ(status.InvalidZ(), false);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::Rejected);

	status = devices::imtq::Status{0x92};
	EXPECT_EQ(status.IsNew(), true);
	EXPECT_EQ(status.InvalidX(), false);
	EXPECT_EQ(status.InvalidY(), false);
	EXPECT_EQ(status.InvalidZ(), true);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::InvalidCommandCode);

	status = devices::imtq::Status{0xA3};
	EXPECT_EQ(status.IsNew(), true);
	EXPECT_EQ(status.InvalidX(), false);
	EXPECT_EQ(status.InvalidY(), true);
	EXPECT_EQ(status.InvalidZ(), false);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::ParameterMissing);

	status = devices::imtq::Status{0xB4};
	EXPECT_EQ(status.IsNew(), true);
	EXPECT_EQ(status.InvalidX(), false);
	EXPECT_EQ(status.InvalidY(), true);
	EXPECT_EQ(status.InvalidZ(), true);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::ParameterInvalid);

	status = devices::imtq::Status{0xC5};
	EXPECT_EQ(status.IsNew(), true);
	EXPECT_EQ(status.InvalidX(), true);
	EXPECT_EQ(status.InvalidY(), false);
	EXPECT_EQ(status.InvalidZ(), false);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::CommandUnavailableInCurrentMode);

	status = devices::imtq::Status{0xD7};
	EXPECT_EQ(status.IsNew(), true);
	EXPECT_EQ(status.InvalidX(), true);
	EXPECT_EQ(status.InvalidY(), false);
	EXPECT_EQ(status.InvalidZ(), true);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::InternalError);

	status = devices::imtq::Status{0xE0};
	EXPECT_EQ(status.IsNew(), true);
	EXPECT_EQ(status.InvalidX(), true);
	EXPECT_EQ(status.InvalidY(), true);
	EXPECT_EQ(status.InvalidZ(), false);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::Accepted);

	status = devices::imtq::Status{0xF1};
	EXPECT_EQ(status.IsNew(), true);
	EXPECT_EQ(status.InvalidX(), true);
	EXPECT_EQ(status.InvalidY(), true);
	EXPECT_EQ(status.InvalidZ(), true);
	EXPECT_EQ(status.CmdError(), devices::imtq::Status::Error::Rejected);
}

TEST(ImtqTestDataStructures, CurrentCalculations)
{
	devices::imtq::CurrentMeasurement current;

	for(uint32_t c = 1; c < 65000; c += 1000) {
		current.setIn0dot1miliAmpsStep(c);
		EXPECT_EQ(current.getIn0dot1miliAmpsStep(), c);
		EXPECT_EQ(current.getInMiliAmpere(), c/10);
	}

	for(uint32_t c = 1; c < 6500; c += 1000) {
		current.setInMiliAmpere(c);
		EXPECT_EQ(current.getIn0dot1miliAmpsStep(), 10*c);
		EXPECT_EQ(current.getInMiliAmpere(), c);
	}
}

class ImtqTest : public testing::Test
{
  public:
    ImtqTest() : imtq(i2c) {}

  protected:
  	devices::imtq::ImtqDriver imtq;
    I2CBusMock i2c;
};


TEST_F(ImtqTest, TestNoOperation)
{
	// accepted
    EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0x02), _))
        .WillOnce(Invoke([](uint8_t /*address*/,
        		            auto /*inData*/,
							auto outData) {
    		outData[0] = 0x02;
    		outData[1] = 0;
            return I2CResult::OK;
        }));

    auto status = imtq.SendNoOperation();
    ASSERT_THAT(status, Eq(true));

	// command rejected
    EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0x02), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
    		outData[0] = 0x02;
    		outData[1] = 1;
            return I2CResult::OK;
        }));

    status = imtq.SendNoOperation();
    ASSERT_THAT(status, Eq(false));

    // bad opcode response
    EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0x02), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
			outData[0] = 0x01;
			outData[1] = 0;
			return I2CResult::OK;
		}));

	status = imtq.SendNoOperation();
	ASSERT_THAT(status, Eq(false));

	// I2C returned fail
	EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0x02), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
			outData[0] = 0x02;
			outData[1] = 0;
			return I2CResult::Failure;
		}));

	status = imtq.SendNoOperation();
	ASSERT_THAT(status, Eq(false));
}


TEST_F(ImtqTest, SoftwareReset)
{
	// reset OK
    EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0xAA), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto /*outData*/) {
            return I2CResult::Nack;
        }));

    auto status = imtq.SoftwareReset();
    ASSERT_THAT(status, Eq(true));

    // fast boot/delay on read
    EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0xAA), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
			outData[0] = 0xFF;
			outData[1] = 0xFF;
			return I2CResult::OK;
		}));

	status = imtq.SoftwareReset();
	ASSERT_THAT(status, Eq(true));

	// I2C returned fail
	EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0xAA), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto /*outData*/) {
			return I2CResult::Failure;
		}));

	status = imtq.SoftwareReset();
	ASSERT_THAT(status, Eq(false));

	// Reset rejected
	EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0xAA), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
			outData[0] = 0xAA;
			outData[1] = static_cast<uint8_t>(devices::imtq::Status::Error::Rejected);
			return I2CResult::OK;
		}));

	status = imtq.SoftwareReset();
	ASSERT_THAT(status, Eq(false));
}

TEST_F(ImtqTest, CancelOperation)
{
	// command rejected
	EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0x03), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
			outData[0] = 0x03;
			outData[1] = 0;
			return I2CResult::OK;
		}));

	auto status = imtq.CancelOperation();
	ASSERT_THAT(status, Eq(true));

	// command rejected
    EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0x03), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
    		outData[0] = 0x03;
    		outData[1] = 1;
            return I2CResult::OK;
        }));

    status = imtq.CancelOperation();
    ASSERT_THAT(status, Eq(false));

    // bad opcode response
    EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0x03), _))
    	.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
			outData[0] = 0x01;
			outData[1] = 0;
			return I2CResult::OK;
		}));

	status = imtq.CancelOperation();
	ASSERT_THAT(status, Eq(false));

	// I2C returned fail
	EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0x03), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
			outData[0] = 0x03;
			outData[1] = 0;
			return I2CResult::Failure;
		}));

	status = imtq.CancelOperation();
	ASSERT_THAT(status, Eq(false));
}

TEST_F(ImtqTest, StartMTMMeasurement)
{
	// command accepted
	EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0x04), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
			outData[0] = 0x04;
			outData[1] = 0;
			return I2CResult::OK;
		}));

	auto status = imtq.StartMTMMeasurement();
	ASSERT_THAT(status, Eq(true));

	// command rejected
    EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0x04), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
    		outData[0] = 0x04;
    		outData[1] = 1;
            return I2CResult::OK;
        }));

    status = imtq.StartMTMMeasurement();
    ASSERT_THAT(status, Eq(false));

    // bad opcode response
    EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0x04), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
			outData[0] = 0x01;
			outData[1] = 0;
			return I2CResult::OK;
		}));

	status = imtq.StartMTMMeasurement();
	ASSERT_THAT(status, Eq(false));

	// I2C returned fail
	EXPECT_CALL(i2c, WriteRead(ImtqAddress, ElementsAre(0x04), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
			outData[0] = 0x04;
			outData[1] = 0;
			return I2CResult::Failure;
		}));

	status = imtq.StartMTMMeasurement();
	ASSERT_THAT(status, Eq(false));
}

TEST_F(ImtqTest, StartActuationCurrent)
{
	using namespace std::chrono_literals;

	// command accepted
	EXPECT_CALL(i2c, WriteRead(ImtqAddress, BeginsWith(0x05), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto inData,
							auto outData) {
			EXPECT_EQ(inData[0], 0x05);

			EXPECT_EQ(inData[1], 0xE8);
			EXPECT_EQ(inData[2], 0x03);
			EXPECT_EQ(inData[3], 0xD0);
			EXPECT_EQ(inData[4], 0x07);
			EXPECT_EQ(inData[5], 0xB8);
			EXPECT_EQ(inData[6], 0x0B);

			EXPECT_EQ(inData[7], 250);
			EXPECT_EQ(inData[8], 0x00);

			outData[0] = 0x05;
			outData[1] = 0;
			return I2CResult::OK;
		}));

	std::array<devices::imtq::CurrentMeasurement, 3> currents;
	currents[0].setInMiliAmpere(100);
	currents[1].setInMiliAmpere(200);
	currents[2].setInMiliAmpere(300);

	auto status = imtq.StartActuationCurrent(currents, 250ms);
	ASSERT_THAT(status, Eq(true));

	// another values
	EXPECT_CALL(i2c, WriteRead(ImtqAddress, BeginsWith(0x05), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto inData,
							auto outData) {
			EXPECT_EQ(inData[0], 0x05);

			EXPECT_EQ(inData[1], 0x38);
			EXPECT_EQ(inData[2], 0xC7);
			EXPECT_EQ(inData[3], 0x58);
			EXPECT_EQ(inData[4], 0x1B);
			EXPECT_EQ(inData[5], 0xC8);
			EXPECT_EQ(inData[6], 0xAF);

			EXPECT_EQ(inData[7], 0x98);
			EXPECT_EQ(inData[8], 0x3A);

			outData[0] = 0x05;
			outData[1] = 0;
			return I2CResult::OK;
		}));

	currents[0].setInMiliAmpere(5100);
	currents[1].setInMiliAmpere(700);
	currents[2].setInMiliAmpere(4500);

	status = imtq.StartActuationCurrent(currents, 15s);
	ASSERT_THAT(status, Eq(true));

	// command rejected
    EXPECT_CALL(i2c, WriteRead(ImtqAddress, BeginsWith(0x05), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
    		outData[0] = 0x05;
    		outData[1] = 1;
            return I2CResult::OK;
        }));

    status = imtq.StartActuationCurrent(currents, 15s);
    ASSERT_THAT(status, Eq(false));

    // bad opcode response
    EXPECT_CALL(i2c, WriteRead(ImtqAddress, BeginsWith(0x05), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
			outData[0] = 0x01;
			outData[1] = 0;
			return I2CResult::OK;
		}));

	status = imtq.StartActuationCurrent(currents, 15s);
	ASSERT_THAT(status, Eq(false));

	// I2C returned fail
	EXPECT_CALL(i2c, WriteRead(ImtqAddress, BeginsWith(0x05), _))
		.WillOnce(Invoke([](uint8_t /*address*/,
							auto /*inData*/,
							auto outData) {
			outData[0] = 0x05;
			outData[1] = 0;
			return I2CResult::Failure;
		}));

	status = imtq.StartActuationCurrent(currents, 15s);
	ASSERT_THAT(status, Eq(false));
}

