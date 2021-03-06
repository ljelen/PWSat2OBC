#include "miniport.h"
#include <string.h>
#include <cstdint>
#include <gsl/span>
#include "base/reader.h"
#include "i2c/i2c.h"
#include "logger/logger.h"

using gsl::span;
using drivers::i2c::II2CBus;
using drivers::i2c::I2CResult;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::duration_cast;
using namespace std::chrono_literals;

/**
 * @brief Enumerator of all supported antenna controller commands.
 * @ingroup AntennaMiniport
 */
enum Command
{
    DEPLOY_ANTENNA = 0xa0,
    START_AUTOMATIC_DEPLOYMENT = 0xa5,
    CANCEL_ANTENNA_DEPLOYMENT = 0xa9,
    RESET = 0xaa,
    DISARM = 0xac,
    ARM = 0xad,
    DEPLOY_ANTENNA_OVERRIDE = 0xb9,
    QUERY_ANTENNA_ACTIVATION_COUNT = 0xaf,
    QUERY_ANTENNA_ACTIVATION_TIME = 0xb3,
    QUERY_TEMPERATURE = 0xc0,
    QUERY_DEPLOYMENT_STATUS = 0xc3
};

/**
 * @brief Maps boolean operation status to OSResult::.
 * @param[in] status Operation status.
 * @return Mapped operation status.
 * @ingroup AntennaMiniport
 */
static inline OSResult MapStatus(I2CResult status)
{
    switch (status)
    {
        case I2CResult::OK:
            return OSResult::Success;
        case I2CResult::Nack:
        case I2CResult::BusErr:
        case I2CResult::ArbLost:
        case I2CResult::UsageFault:
        case I2CResult::SwFault:
        case I2CResult::LineLatched:
        case I2CResult::Failure:
            return OSResult::IOError;
        case I2CResult::Timeout:
            return OSResult::Timeout;
        default:
        case I2CResult::LineAlreadyLatched:
            return OSResult::ProtocolError;
    }
}

/**
 * @brief Sends requested command via the passed miniport driver.
 * @param[in] error Aggregator for error counter.
 * @param[in] bus Bus that should be used to communicate with hardware.
 * @param[in] channel Current hardware channel.
 * @param[in] command Command to be sent.
 * @return Operation status. True on success, false otherwise.
 * @ingroup AntennaMiniport
 */
static OSResult SendCommand(error_counter::AggregatedErrorCounter& error, II2CBus* bus, AntennaChannel channel, Command command)
{
    uint8_t data = (uint8_t)command;
    const I2CResult result = bus->Write(channel, span<const uint8_t>(&data, 1));
    const bool status = (result == I2CResult::OK);
    if (!status)
    {
        LOGF(LOG_LEVEL_ERROR, "[ant] Unable to send command %d to %d, Reason: %d", command, channel, num(result));
    }

    return MapStatus(result) >> error;
}

/**
 * @brief Sends requested command via the passed miniport driver and reads hardware response
 * @param[in] error Aggregator for error counter.
 * @param[in] bus Bus that should be used to communicate with hardware.
 * @param[in] channel Current hardware channel.
 * @param[in] command Command to be sent.
 * @param[out] outBuffer Buffer that should be filled with response from hardware.
 * @param[in] outBufferSize Size in bytes of the outBuffer buffer.
 * @return Operation status. True on success, false otherwise.
 * @ingroup AntennaMiniport
 */
static OSResult SendCommandWithResponse(error_counter::AggregatedErrorCounter& error,
    II2CBus* bus,
    AntennaChannel channel,
    Command command,
    uint8_t* outBuffer,
    uint8_t outBufferSize //
    )
{
    const I2CResult result =
        bus->WriteRead(channel, span<const uint8_t>(reinterpret_cast<uint8_t*>(&command), 1), span<uint8_t>(outBuffer, outBufferSize));
    const bool status = (result == I2CResult::OK);
    if (!status)
    {
        LOGF(LOG_LEVEL_ERROR, "[ant] Unable to send command %d to %d, Reason: %d", command, channel, num(result));
    }

    return MapStatus(result) >> error;
}

OSResult AntennaMiniportDriver::Reset(            //
    error_counter::AggregatedErrorCounter& error, //
    II2CBus* communicationBus,                    //
    AntennaChannel channel                        //
    )
{
    return SendCommand(error, communicationBus, channel, RESET);
}

OSResult AntennaMiniportDriver::ArmDeploymentSystem( //
    error_counter::AggregatedErrorCounter& error,    //
    II2CBus* communicationBus,                       //
    AntennaChannel channel                           //
    )
{
    return SendCommand(error, communicationBus, channel, ARM);
}

OSResult AntennaMiniportDriver::DisarmDeploymentSystem( //
    error_counter::AggregatedErrorCounter& error,       //
    II2CBus* communicationBus,                          //
    AntennaChannel channel                              //
    )
{
    return SendCommand(error, communicationBus, channel, DISARM);
}

static inline uint8_t GetOverrideFlag(bool override)
{
    return override ? (DEPLOY_ANTENNA_OVERRIDE - DEPLOY_ANTENNA) : 0;
}

OSResult AntennaMiniportDriver::DeployAntenna(    //
    error_counter::AggregatedErrorCounter& error, //
    II2CBus* communicationBus,                    //
    AntennaChannel channel,                       //
    AntennaId antennaId,                          //
    milliseconds timeout,                         //
    bool override                                 //
    )
{
    uint8_t buffer[2];
    buffer[0] = (uint8_t)(DEPLOY_ANTENNA + antennaId + GetOverrideFlag(override));
    buffer[1] = (uint8_t)duration_cast<seconds>(timeout).count();
    return MapStatus(communicationBus->Write(channel, buffer)) >> error;
}

OSResult AntennaMiniportDriver::InitializeAutomaticDeployment( //
    error_counter::AggregatedErrorCounter& error,              //
    II2CBus* communicationBus,                                 //
    AntennaChannel channel,                                    //
    milliseconds timeout                                       //
    )
{
    uint8_t buffer[2];
    buffer[0] = static_cast<uint8_t>(START_AUTOMATIC_DEPLOYMENT);
    buffer[1] = static_cast<uint8_t>(duration_cast<seconds>(timeout).count() >> 2);
    return MapStatus(communicationBus->Write(channel, buffer)) >> error;
}

OSResult AntennaMiniportDriver::CancelAntennaDeployment( //
    error_counter::AggregatedErrorCounter& error,        //
    II2CBus* communicationBus,                           //
    AntennaChannel channel                               //
    )
{
    return SendCommand(error, communicationBus, channel, CANCEL_ANTENNA_DEPLOYMENT);
}

OSResult AntennaMiniportDriver::GetDeploymentStatus( //
    error_counter::AggregatedErrorCounter& error,    //
    II2CBus* communicationBus,                       //
    AntennaChannel channel,                          //
    AntennaDeploymentStatus* telemetry               //
    )
{
    uint8_t output[2];
    memset(telemetry, 0, sizeof(*telemetry));
    const OSResult result = SendCommandWithResponse(error,
        communicationBus,
        channel,
        QUERY_DEPLOYMENT_STATUS,
        output,
        sizeof(output) //
        );

    if (OS_RESULT_FAILED(result))
    {
        return result >> error;
    }

    Reader reader(output);
    const uint16_t value = reader.ReadWordLE();

    telemetry->DeploymentStatus[0] = IS_BIT_CLEAR(value, 15); // (value & 0x8000) == 0;
    telemetry->DeploymentStatus[1] = IS_BIT_CLEAR(value, 11); // (value & 0x0800) == 0;
    telemetry->DeploymentStatus[2] = IS_BIT_CLEAR(value, 7);  // (value & 0x0080) == 0;
    telemetry->DeploymentStatus[3] = IS_BIT_CLEAR(value, 3);  // (value & 0x0008) == 0;

    telemetry->IsDeploymentActive[0] = IS_BIT_SET(value, 13); //(value & 0x2000) != 0;
    telemetry->IsDeploymentActive[1] = IS_BIT_SET(value, 9);  //(value & 0x0200) != 0;
    telemetry->IsDeploymentActive[2] = IS_BIT_SET(value, 5);  //(value & 0x0020) != 0;
    telemetry->IsDeploymentActive[3] = IS_BIT_SET(value, 1);  //(value & 0x0002) != 0;

    telemetry->DeploymentTimeReached[0] = IS_BIT_SET(value, 14);
    telemetry->DeploymentTimeReached[1] = IS_BIT_SET(value, 10);
    telemetry->DeploymentTimeReached[2] = IS_BIT_SET(value, 6);
    telemetry->DeploymentTimeReached[3] = IS_BIT_SET(value, 2);

    telemetry->IgnoringDeploymentSwitches = IS_BIT_SET(value, 8); //(value & 0x0100) != 0;
    telemetry->DeploymentSystemArmed = IS_BIT_SET(value, 0);      //(value & 0x0001) != 0;
    telemetry->IsIndependentBurnActive = IS_BIT_SET(value, 4);

    return OSResult::Success >> error;
}

OSResult AntennaMiniportDriver::GetAntennaActivationCount( //
    error_counter::AggregatedErrorCounter& error,          //
    II2CBus* communicationBus,                             //
    AntennaChannel channel,                                //
    AntennaId antennaId,                                   //
    uint8_t* count                                         //
    )
{
    uint8_t output;
    const OSResult result = SendCommandWithResponse(error,
        communicationBus,
        channel,
        (Command)(QUERY_ANTENNA_ACTIVATION_COUNT + antennaId),
        &output,
        sizeof(output) //
        );

    if (OS_RESULT_FAILED(result))
    {
        *count = 0;
        return result >> error;
    }

    *count = output;
    return OSResult::Success >> error;
}

OSResult AntennaMiniportDriver::GetAntennaActivationTime( //
    error_counter::AggregatedErrorCounter& error,         //
    II2CBus* communicationBus,                            //
    AntennaChannel channel,                               //
    AntennaId antennaId,                                  //
    milliseconds* span                                    //
    )
{
    uint8_t output[2];

    const OSResult result = SendCommandWithResponse(error,
        communicationBus,
        channel,
        (Command)(QUERY_ANTENNA_ACTIVATION_TIME + antennaId),
        output,
        sizeof(output) //
        );

    if (OS_RESULT_FAILED(result))
    {
        *span = 0ms;
        return OSResult::IOError >> error;
    }

    Reader reader(output);
    const uint16_t value = reader.ReadWordLE();
    *span = milliseconds(value * 50);
    return OSResult::Success >> error;
}

OSResult AntennaMiniportDriver::GetTemperature(   //
    error_counter::AggregatedErrorCounter& error, //
    II2CBus* communicationBus,                    //
    AntennaChannel channel,                       //
    uint16_t* temperature                         //
    )
{
    uint8_t output[2];
    *temperature = 0;
    const OSResult result = SendCommandWithResponse(error,
        communicationBus,
        channel,
        QUERY_TEMPERATURE,
        output,
        sizeof(output) //
        );

    if (OS_RESULT_FAILED(result))
    {
        return OSResult::IOError >> error;
    }

    Reader reader(output);
    const uint16_t value = reader.ReadWordBE();
    *temperature = value & 0x3ff;
    return OSResult::Success >> error;
}
