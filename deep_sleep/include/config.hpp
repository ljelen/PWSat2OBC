#pragma once

#include <chrono>
#include <cstdint>
#include <em_burtc.h>

struct Config
{

#ifdef FAST_DEEP_SLEEP
    static constexpr std::uint32_t BuRTCCompareValue = 1024;
    static constexpr std::chrono::minutes ScrubbingInterval = std::chrono::minutes(1);
#else
    static constexpr std::uint32_t BuRTCCompareValue = 10240;
    static constexpr std::chrono::minutes ScrubbingInterval = std::chrono::minutes(30);
#endif

    static constexpr uint32_t PrescalerDivider = burtcClkDiv_128;
    static constexpr auto TickLength = std::chrono::milliseconds(1000 * BuRTCCompareValue * PrescalerDivider / 32768);
};
