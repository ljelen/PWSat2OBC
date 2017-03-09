#include "base/os.h"

#include "eps.h"

#include "i2c/i2c.h"
#include "logger/logger.h"

#include "system.h"

using drivers::i2c::II2CBus;
using drivers::i2c::I2CResult;
using namespace std::chrono_literals;

#define EPS_ADDRESS 12

typedef enum {
    EPS_LCL_SAIL_0 = 0,
    EPS_LCL_SAIL_1 = 1,
} EpsLcl;

static II2CBus* i2c;

static bool epsControlLCL(EpsLcl lcl, uint8_t state)
{
    uint8_t data[] = {static_cast<uint8_t>(1 + lcl), state};
    const I2CResult result = i2c->Write(EPS_ADDRESS, data);

    if (result != I2CResult::OK)
    {
        LOGF(LOG_LEVEL_ERROR, "[EPS] ControlLCL %d to state %d failed: %d", lcl, state, num(result));
    }

    return result == I2CResult::OK;
}

bool EpsOpenSail(void)
{
    LOG(LOG_LEVEL_INFO, "[EPS] Opening sail");

    if (!epsControlLCL(EPS_LCL_SAIL_0, true))
    {
        return false;
    }
    System::SleepTask(100ms);

    if (!epsControlLCL(EPS_LCL_SAIL_0, false))
    {
        return false;
    }
    System::SleepTask(100ms);

    if (!epsControlLCL(EPS_LCL_SAIL_1, true))
    {
        return false;
    }
    System::SleepTask(100ms);

    if (!epsControlLCL(EPS_LCL_SAIL_1, false))
    {
        return false;
    }
    System::SleepTask(100ms);

    return true;
}

bool EpsTriggerSystemPowerCycle(void)
{
    uint8_t data[] = {0xA0};
    const I2CResult result = i2c->Write(EPS_ADDRESS, data);

    if (result != I2CResult::OK)
    {
        LOGF(LOG_LEVEL_ERROR, "[EPS] EpsTriggerSystemPowerCycle failed: %d", num(result));
    }

    return result == I2CResult::OK;
}

void EpsInit(II2CBus* bus)
{
    i2c = bus;
}

namespace devices
{
    namespace eps
    {
        static constexpr auto PowerCycleTimeout = 3s + 1s;

        enum class Command
        {
            PowerCycle = 0xE0,
            EnableLCL = 0xE1,
            DisableLCL = 0xE2,
            EnableBurnSwitch = 0xE3,
            DisableOverheatSubmode = 0xE4,
        };

        EPSDriver::EPSDriver(drivers::i2c::I2CInterface& i2c) : _i2c(i2c)
        {
        }

        Option<hk::HouseheepingControllerA> EPSDriver::ReadHousekeepingA()
        {
            std::array<std::uint8_t, 1> command{0x0};
            std::array<std::uint8_t, 72> response;

            auto result = this->WriteRead(Controller::A, command, response);

            if (result != I2CResult::OK)
            {
                return None<hk::HouseheepingControllerA>();
            }

            hk::HouseheepingControllerA housekeeping;
            Reader r(response);
            r.ReadByte(); // error flag register

            if (!housekeeping.ReadFrom(r))
            {
                return None<hk::HouseheepingControllerA>();
            }

            return Some(housekeeping);
        }

        Option<hk::HouseheepingControllerB> EPSDriver::ReadHousekeepingB()
        {
            std::array<std::uint8_t, 1> command{0x0};
            std::array<std::uint8_t, 16> response;

            auto result = this->WriteRead(Controller::B, command, response);

            if (result != I2CResult::OK)
            {
                return None<hk::HouseheepingControllerB>();
            }

            hk::HouseheepingControllerB housekeeping;
            Reader r(response);

            r.ReadByte(); // error flag register

            if (!housekeeping.ReadFrom(r))
            {
                return None<hk::HouseheepingControllerB>();
            }

            return Some(housekeeping);
        }

        bool EPSDriver::PowerCycleA()
        {
            std::array<std::uint8_t, 1> command{num(Command::PowerCycle)};

            if (this->Write(Controller::A, command) != I2CResult::OK)
            {
                return false;
            }

            System::SleepTask(PowerCycleTimeout);

            return false;
        }

        bool EPSDriver::PowerCycleB()
        {
            std::array<std::uint8_t, 1> command{num(Command::PowerCycle)};

            if (this->Write(Controller::B, command) != I2CResult::OK)
            {
                return false;
            }

            System::SleepTask(PowerCycleTimeout);

            return false;
        }

        bool EPSDriver::PowerCycle()
        {
            this->PowerCycleA();
            this->PowerCycleB();
            return false;
        }

        ErrorCode EPSDriver::EnableLCL(LCL lcl)
        {
            auto controller = (num(lcl) & 0xF0) == 0 ? Controller::A : Controller::B;
            auto lclId = static_cast<std::uint8_t>((num(lcl) & 0x0F));

            std::array<std::uint8_t, 2> command{num(Command::EnableLCL), lclId};

            if (this->Write(controller, command) != I2CResult::OK)
            {
                return ErrorCode::CommunicationFailure;
            }

            return GetErrorCode(controller);
        }

        ErrorCode EPSDriver::DisableLCL(LCL lcl)
        {
            auto controller = (num(lcl) & 0xF0) == 0 ? Controller::A : Controller::B;
            auto lclId = static_cast<std::uint8_t>((num(lcl) & 0x0F));

            std::array<std::uint8_t, 2> command{num(Command::DisableLCL), lclId};

            if (this->Write(controller, command) != I2CResult::OK)
            {
                return ErrorCode::CommunicationFailure;
            }

            return GetErrorCode(controller);
        }

        I2CResult EPSDriver::Write(Controller controller, const gsl::span<std::uint8_t> inData)
        {
            switch (controller)
            {
                case Controller::A:
                    return this->_i2c.Bus.Write(ControllerA, inData);
                case Controller::B:
                    return this->_i2c.Payload.Write(ControllerB, inData);
                default:
                    return I2CResult::Failure;
            }
        }

        bool EPSDriver::DisableOverheatSubmodeA()
        {
            std::array<std::uint8_t, 1> command{num(Command::DisableOverheatSubmode)};
            return this->Write(Controller::A, command) == I2CResult::OK;
        }

        bool EPSDriver::DisableOverheatSubmodeB()
        {
            std::array<std::uint8_t, 1> command{num(Command::DisableOverheatSubmode)};
            return this->Write(Controller::B, command) == I2CResult::OK;
        }

        I2CResult EPSDriver::WriteRead(Controller controller, gsl::span<const uint8_t> inData, gsl::span<uint8_t> outData)
        {
            switch (controller)
            {
                case Controller::A:
                    return this->_i2c.Bus.WriteRead(ControllerA, inData, outData);
                case Controller::B:
                    return this->_i2c.Payload.WriteRead(ControllerB, inData, outData);
                default:
                    return I2CResult::Failure;
            }
        }

        ErrorCode EPSDriver::EnableBurnSwitch(bool main, BurnSwitch burnSwitch)
        {
            std::array<std::uint8_t, 2> command{num(Command::EnableBurnSwitch), num(burnSwitch)};

            auto controller = main ? Controller::A : Controller::B;

            if (this->Write(controller, command) != I2CResult::OK)
            {
                return ErrorCode::CommunicationFailure;
            }

            return GetErrorCode(controller);
        }

        ErrorCode EPSDriver::GetErrorCode(Controller controller)
        {
            std::array<std::uint8_t, 1> command{0x0};
            std::array<std::uint8_t, 1> response;

            auto r = this->WriteRead(controller, command, response);

            if (r != I2CResult::OK)
            {
                return ErrorCode::CommunicationFailure;
            }

            return static_cast<ErrorCode>(response[0]);
        }

        ErrorCode EPSDriver::GetErrorCodeA()
        {
            return GetErrorCode(Controller::A);
        }

        ErrorCode EPSDriver::GetErrorCodeB()
        {
            return GetErrorCode(Controller::B);
        }
    }
}
