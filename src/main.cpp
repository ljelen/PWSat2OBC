#include <cstdio>
#include <cstring>
#include <em_acmp.h>
#include <em_chip.h>
#include <em_cmu.h>
#include <em_dbg.h>
#include <em_device.h>
#include <em_emu.h>
#include <em_gpio.h>
#include <em_timer.h>
#include <gsl/span>
#include <em_system.h>

#include <FreeRTOS.h>
#include <FreeRTOSConfig.h>
#include <task.h>

#include "SwoEndpoint/SwoEndpoint.h"
#include "base/ecc.h"
#include "base/os.h"
#include "blink.hpp"
#include "boot/params.hpp"
#include "dmadrv.h"
#include "efm_support/api.h"
#include "efm_support/clock.h"
#include "fs/fs.h"
#include "gpio/gpio.h"
#include "i2c/i2c.h"
#include "logger/logger.h"
#include "mcu/io_map.h"
#include "mission.h"
#include "obc.h"
#include "obc/ObcState.hpp"
#include "power_eps/power_eps.h"
#include "swo/swo.h"
#include "system.h"
#include "terminal.h"
#include "watchdog/internal.hpp"

using services::time::TimeProvider;
using namespace std::chrono_literals;

OBC Main;

telemetry::ObcTelemetryAcquisition TelemetryAcquisition(Main.Hardware.CommDriver,
    std::make_tuple(std::ref(Main.fs), mission::TelemetryConfiguration{"/telemetry.current", "/telemetry.previous", 512_KB, 10}),
    Main.Hardware.Gyro,
    Main.Fdir,
    Main.Hardware.EPS,
    Main.Experiments.ExperimentsController,
    Main.Hardware.MCUTemperature,
    Mission,
    0,
    Main.fs,
    Main.timeProvider,
    Main.Hardware.rtc,
    Main.BootTable,
    Main.Scrubbing,
    0,
    0);

mission::ObcMission Mission(std::tie(Main.timeProvider, Main.Hardware.rtc),
    0,
    std::tie(Main.Hardware.antennaDriver, Main.PowerControlInterface),
    Main.Hardware.CommDriver,
    Main.PowerControlInterface,
    Main.adcs.GetAdcsCoordinator(),
    Main.Experiments.ExperimentsController,
    std::make_pair(std::ref(Main.Hardware.CommDriver), std::ref(TelemetryAcquisition)),
    Main.Fdir,
    std::tie(Main.Hardware.PersistentStorage, PersistentStateBaseAddress),
    Main.fs,
    Main.Hardware.EPS);

const int __attribute__((used)) uxTopUsedPriority = configMAX_PRIORITIES;

extern "C" void vApplicationIdleHook(void)
{
    EMU_EnterEM1();
}

extern "C" void vApplicationTickHook(void)
{
    ExternalWatchdog::Kick();
}

void I2C0_IRQHandler(void)
{
    Main.Hardware.I2C.Peripherals[0].Driver.IRQHandler();
}

void I2C1_IRQHandler(void)
{
    Main.Hardware.I2C.Peripherals[1].Driver.IRQHandler();
}

void BURTC_IRQHandler(void)
{
    Main.Hardware.Burtc.IRQHandler();
}

void GPIO_EVEN_IRQHandler(void)
{
    std::uint32_t irq = GPIO_IntGet();
    if (irq & Main.Hardware.PayloadInterruptDriver.IRQMask())
    {
        Main.Hardware.PayloadDriver.IRQHandler();
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "Unknown GPIO interrupt occurred");
        GPIO_IntClear(irq);
        System::EndSwitchingISR();
    }
}

void LESENSE_IRQHandler()
{
    Main.Hardware.Terminal.OnWakeUpInterrupt();
    System::EndSwitchingISR();
}

void TIMER0_IRQHandler()
{
    Scrubber::Scrub();
    TIMER_IntClear(io_map::RAMScrubbing::TimerHW, TIMER_IFC_OF);
}

void ACMP0_IRQHandler()
{
    if (has_flag(ACMP_IntGet(ACMP0), ACMP_IF_EDGE))
    {
        Main.Memory.HandleLatchup(obc::MemoryModule::SRAM1);
        ACMP_IntClear(ACMP0, ACMP_IFC_EDGE);
    }

    if (has_flag(ACMP_IntGet(ACMP1), ACMP_IF_EDGE))
    {
        Main.Memory.HandleLatchup(obc::MemoryModule::SRAM2);
        ACMP_IntClear(ACMP1, ACMP_IFC_EDGE);
    }
}

__attribute__((optimize("O3"))) void UART1_RX_IRQHandler()
{
    Main.Hardware.Terminal.OnReceived();
}

static void InitSwoEndpoint(void)
{
    void* swoEndpointHandle = SwoEndpointInit();
    const bool result = LogAddEndpoint(SwoGetEndpoint(swoEndpointHandle), swoEndpointHandle, LOG_LEVEL_TRACE);
    if (!result)
    {
        SwoPutsOnChannel(0, "Unable to attach swo endpoint to logger. ");
    }
}

static void ObcInitTask(void* param)
{
    ExternalWatchdog::Enable();

    LOG(LOG_LEVEL_INFO, "Starting initialization task...");
    LOGF(LOG_LEVEL_INFO, "Requested runlevel %d", num(boot::RequestedRunlevel));

    auto obc = static_cast<OBC*>(param);

    if (boot::RequestedRunlevel >= boot::Runlevel::Runlevel1)
    {
        if (OS_RESULT_FAILED(obc->InitializeRunlevel1()))
        {
            LOG(LOG_LEVEL_ERROR, "Unable to initialize runlevel 1. ");
        }
        else
        {
            LOG(LOG_LEVEL_ERROR, "Runlevel 1 initialized");
        }
    }

    if (boot::RequestedRunlevel >= boot::Runlevel::Runlevel2)
    {
        if (OS_RESULT_FAILED(obc->InitializeRunlevel2()))
        {
            LOG(LOG_LEVEL_ERROR, "Unable to initialize runlevel 2. ");
        }
        else
        {
            LOG(LOG_LEVEL_ERROR, "Runlevel 2 initialized");
        }
    }

    System::SuspendTask(NULL);
}

static void SetupRAMScrubbing()
{
    CMU_ClockEnable(efm::Clock(io_map::RAMScrubbing::TimerHW), true);

    TIMER_Init_TypeDef init = TIMER_INIT_DEFAULT;
    init.count2x = false;
    init.enable = false;
    init.mode = timerModeUp;
    init.prescale = io_map::RAMScrubbing::Prescaler;

    TIMER_Init(TIMER0, &init);

    TIMER_TopSet(TIMER0, io_map::RAMScrubbing::TimerTop);

    TIMER_IntEnable(io_map::RAMScrubbing::TimerHW, TIMER_IEN_OF);

    NVIC_EnableIRQ(io_map::RAMScrubbing::IRQ);

    TIMER_Enable(io_map::RAMScrubbing::TimerHW, true);
}

void SetupHardware(void)
{
    CMU_ClockEnable(cmuClock_GPIO, true);
    CMU_ClockEnable(cmuClock_DMA, true);

    CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_HFCLKLE);
    CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_HFCLKLE);

#ifndef SLOWER_CLOCK
    CMU_OscillatorEnable(cmuOsc_HFXO, true, true);
    CMU_ClockSelectSet(cmuClock_HF, cmuSelect_HFXO);
    CMU_OscillatorEnable(cmuOsc_HFRCO, false, true);
#endif

    SetupRAMScrubbing();
}

extern "C" void __libc_init_array(void);

int main(void)
{
    memset(&Main, 0, sizeof(Main));

    __libc_init_array();

    CHIP_Init();

    drivers::watchdog::InternalWatchdog::Disable();

    SetupHardware();

    SwoEnable();

    LogInit(LOG_LEVEL_DEBUG);
    InitSwoEndpoint();

    DMADRV_Init();

    if (!boot::IsBootInformationAvailable())
    {
        LOGF(LOG_LEVEL_WARNING,
            "No boot information from bootloader (expected: 0x%lX, got: 0x%lX)",
            boot::BootloaderMagicNumber,
            boot::MagicNumber);

        boot::RequestedRunlevel = boot::Runlevel::Runlevel2;
        boot::Index = 0;
        boot::BootReason = boot::Reason::BootToUpper;
        boot::ClearStateOnStartup = false;
    }
    else
    {
        LOG(LOG_LEVEL_DEBUG, "Received boot information from bootloader");
    }

    Main.InitializeRunlevel0();

    SwoPutsOnChannel(0, "Hello I'm PW-SAT2 OBC\n");

    Main.Hardware.Pins.TimeIndicator.High();
    Main.Hardware.Pins.BootIndicator.High();

    InitializeBlink();
    System::CreateTask(ObcInitTask, "Init", 8_KB, &Main, TaskPriority::P14, &Main.initTask);

    System::RunScheduler();

    Main.Hardware.Pins.BootIndicator.Toggle();

    return 0;
}
