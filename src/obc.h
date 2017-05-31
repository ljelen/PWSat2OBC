#ifndef OBC_H
#define OBC_H

#include <array>
#include <atomic>
#include <cstdint>
#include <gsl/span>

#include "adcs/AdcsCoordinator.hpp"
#include "adcs/AdcsExperimental.hpp"

#include "base/os.h"
#include "experiment/fibo/fibo.h"
#include "fs/fs.h"
#include "fs/yaffs.h"
#include "leuart/line_io.h"
#include "n25q/n25q.h"
#include "n25q/yaffs.h"
#include "obc/adcs.hpp"
#include "obc/communication.h"
#include "obc/experiments.hpp"
#include "obc/fdir.hpp"
#include "obc/hardware.h"
#include "obc/storage.h"
#include "power_eps/power_eps.h"
#include "program_flash/boot_table.hpp"
#include "scrubber/ram.hpp"
#include "spi/efm.h"
#include "terminal/terminal.h"
#include "time/timer.h"
#include "utils.h"

/**
 * @defgroup obc OBC structure
 *
 * @{
 */

/**
 * @brief Object that describes global OBC state including drivers.
 */
struct OBC
{
  public:
    /** @brief State flag: OBC initialization finished */
    static constexpr OSEventBits InitializationFinishedFlag = 1;

    /** @brief Constructs @ref OBC object  */
    OBC();

    /** @brief Performs OBC initialization at very early stage of boot process */
    void InitializeRunlevel0();

    /**
     * @brief Initialize OBC at runlevel 1
     * @return Operation result
     */
    OSResult InitializeRunlevel1();

    /**
     * @brief Initialize OBC at runlevel 1
     * @return Operation result
     */
    OSResult InitializeRunlevel2();

    /**
     * @brief Returns current LineIO implementation
     * @return Line IO implementation
     */
    inline LineIO& GetLineIO();

    /** @brief File system object */
    services::fs::YaffsFileSystem fs;

    /** @brief Handle to OBC initialization task. */
    OSTaskHandle initTask;

    /** @brief Flag indicating that OBC software has finished initialization process. */
    EventGroup StateFlags;

    /** @brief Boot Table */
    program_flash::BootTable BootTable;

    /** @brief Persistent timer that measures mission time. */
    services::time::TimeProvider timeProvider;

    /** @brief OBC hardware */
    obc::OBCHardware Hardware;

    /** @brief Standard text based IO. */
    LineIO IO;

    /** @brief Power control interface */
    services::power::EPSPowerControl PowerControlInterface;

    /** @brief FDIR mechanisms */
    obc::FDIR Fdir;

    /** @brief OBC storage */
    obc::OBCStorage Storage;

    /** @brief Adcs subsytem for obc. */
    obc::Adcs adcs;

    /** @brief Experiments */
    obc::OBCExperiments Experiments;

    /** @brief Overall satellite <-> Earth communication */
    obc::OBCCommunication Communication;

    /** @brief Terminal object. */
    Terminal terminal;
};

LineIO& OBC::GetLineIO()
{
#ifdef USE_LEUART
    return this->IO;
#else
    return this->Hardware.UARTDriver.GetLineIO();
#endif
}

/** @brief Global OBC object. */
extern OBC Main;

/** @brief RAM Scrubber */
using Scrubber =
    scrubber::RAMScrubber<io_map::RAMScrubbing::MemoryStart, io_map::RAMScrubbing::MemorySize, io_map::RAMScrubbing::CycleSize>;

static constexpr std::uint32_t PersistentStateBaseAddress = 4;

/** @} */

#endif
