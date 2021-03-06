#ifndef LIBS_STATE_INCLUDE_STATE_STRUCT_H_
#define LIBS_STATE_INCLUDE_STATE_STRUCT_H_

#pragma once

#include <chrono>
#include "LockablePersistentState.hpp"
#include "StatePolicies.hpp"
#include "adcs/AdcsState.hpp"
#include "adcs/adcs.hpp"
#include "antenna/AntennaConfiguration.hpp"
#include "antenna/AntennaState.hpp"
#include "base/os.h"
#include "comm/MessageState.hpp"
#include "experiments/experiments.h"
#include "fdir/ErrorCountersState.hpp"
#include "fwd.hpp"
#include "sail/SailState.hpp"
#include "time/TimeCorrectionConfiguration.hpp"
#include "time/TimeState.hpp"

/**
 * @ingroup StateDef
 * @brief State of the satellite
 */
struct SystemState
{
    SystemState();

    /** @brief Current time */
    std::chrono::milliseconds Time;

    /** @brief Current antenna deployment state. */
    state::AntennaState AntennaState;

    /** @brief Current adcs operational mode. */
    adcs::AdcsMode AdcsMode;

    /**
     * @brief Experiment controller status
     */
    experiments::ExperimentState Experiment;

    /**
     * @brief Satellite's persistent state.
     */
    state::SystemPersistentState PersistentState;
};

#endif /* LIBS_STATE_INCLUDE_STATE_STRUCT_H_ */
