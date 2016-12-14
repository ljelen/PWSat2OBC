#include "AntennaMock.hpp"

static OSResult Reset(struct AntennaDriver* driver, AntennaChannel channel)
{
    return static_cast<AntennaMock*>(driver)->Reset(channel);
}

static OSResult HardReset(struct AntennaDriver* driver)
{
    return static_cast<AntennaMock*>(driver)->HardReset();
}

static OSResult FinishDeployment(struct AntennaDriver* driver, AntennaChannel channel)
{
    return static_cast<AntennaMock*>(driver)->FinishDeployment(channel);
}

static OSResult DeployAntenna(struct AntennaDriver* driver,
    AntennaChannel channel,
    AntennaId antennaId,
    TimeSpan timeout,
    bool overrideSwitches //
    )
{
    return static_cast<AntennaMock*>(driver)->DeployAntenna(channel, antennaId, timeout, overrideSwitches);
}

static OSResult GetDeploymentStatus(struct AntennaDriver* driver, AntennaChannel channel, AntennaDeploymentStatus* telemetry)
{
    return static_cast<AntennaMock*>(driver)->GetDeploymentStatus(channel, telemetry);
}

static OSResult GetTemperature(struct AntennaDriver* driver, AntennaChannel channel, uint16_t* temperature)
{
    return static_cast<AntennaMock*>(driver)->GetTemperature(channel, temperature);
}

static AntennaTelemetry GetTelemetry(struct AntennaDriver* driver)
{
    return static_cast<AntennaMock*>(driver)->GetTelemetry();
}

AntennaMock::AntennaMock()
{
    AntennaDriver::Reset = ::Reset;
    AntennaDriver::HardReset = ::HardReset;
    AntennaDriver::FinishDeployment = ::FinishDeployment;
    AntennaDriver::DeployAntenna = ::DeployAntenna;
    AntennaDriver::GetDeploymentStatus = ::GetDeploymentStatus;
    AntennaDriver::GetTemperature = ::GetTemperature;
    AntennaDriver::GetTelemetry = ::GetTelemetry;
}