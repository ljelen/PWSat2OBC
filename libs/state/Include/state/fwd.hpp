#ifndef LIBS_STATE_FWD_HPP
#define LIBS_STATE_FWD_HPP

#pragma once

namespace state
{
    class AntennaConfiguration;
    class AntennaState;
    class TimeState;

    struct NoTrackingStatePolicy;
    class StateTrackingPolicy;
    template <typename StatePolicy, typename... Parts> class PersistentState;

    typedef PersistentState<StateTrackingPolicy, AntennaConfiguration, TimeState> SystemPersistentState;
}

class SystemState;

#endif