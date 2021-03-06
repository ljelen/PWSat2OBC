#ifndef LIBS_TELEMETRY_STATE_HPP
#define LIBS_TELEMETRY_STATE_HPP

#pragma once

#include "BasicTelemetry.hpp"
#include "ErrorCounters.hpp"
#include "Experiments.hpp"
#include "ImtqTelemetry.hpp"
#include "SystemStartup.hpp"
#include "Telemetry.hpp"
#include "TimeTelemetry.hpp"
#include "antenna/telemetry.hpp"
#include "comm/CommTelemetry.hpp"
#include "fwd.hpp"
#include "gyro/telemetry.hpp"
#include "state/time/TimeState.hpp"
#include "system.h"

namespace telemetry
{
    /**
     * @brief This type represents state of telemetry acquisition loop.
     * @ingroup telemetry
     */
    struct TelemetryState
    {
        /**
         * @brief Initializes telemetry state.
         * @return Operation status, true in case of success, false otherwise.
         */
        bool Initialize();

        /**
         * @brief Container for all telemetry elements currently collected by acquisition loop.
         */
        ManagedTelemetry telemetry;

        /**
         * @brief Handle to semaphore that protects access to serialized telemetry.
         */
        OSSemaphoreHandle bufferLock;

        /**
         * @brief Buffer that contains serialized state of the last seen telemetry state.
         */
        std::array<std::uint8_t, ManagedTelemetry::TotalSerializedSize> lastSerializedTelemetry;
    };

    static_assert(ProgramState::BitSize() == 16, "Invalid serialized size");
    static_assert(FlashPrimarySlotsScrubbing::BitSize() == 3, "Invalid serialized size");
    static_assert(FlashSecondarySlotsScrubbing::BitSize() == 3, "Invalid serialized size");
    static_assert(RAMScrubbing::BitSize() == 32, "Invalid serialized size");
    static_assert(FileSystemTelemetry::BitSize() == 32, "Invalid serialized size");
    static_assert(OSState::BitSize() == 22, "Invalid serialized size");
    static_assert(GpioState::BitSize() == 1, "Invalid serialized size");
    static_assert(McuTemperature::BitSize() == 12, "Invalid serialized size");
    static_assert(ImtqMagnetometerMeasurements::BitSize() == 96, "Invalid serialized size");
    static_assert(ImtqCoilsActive::BitSize() == 1, "Invalid serialized size");
    static_assert(ImtqDipoles::BitSize() == 48, "Invalid serialized size");
    static_assert(ImtqBDotTelemetry::BitSize() == 96, "Invalid serialized size");
    static_assert(ImtqCoilCurrent::BitSize() == 48, "Invalid serialized size");
    static_assert(ImtqCoilTemperature::BitSize() == 48, "Invalid serialized size");
    static_assert(ImtqStatus::BitSize() == 8, "Invalid serialized size");
    static_assert(ImtqSelfTest::BitSize() == 64, "Invalid serialized size");

    static_assert(ManagedTelemetry::TotalSerializedSize <= 230, "Telemetry is too large");
    static_assert(ManagedTelemetry::PayloadSize == 1832, "Invalid Telemetry Size");
}

#endif
