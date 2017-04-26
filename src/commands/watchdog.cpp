#include "watchdog/watchdog.hpp"
#include <FreeRTOS.h>
#include <cstdint>
#include "obc.h"

using drivers::watchdog::InternalWatchdog;

template <typename Watchdog> static void Handle(const char* cmd)
{
    if (strcmp(cmd, "enable") == 0)
    {
        Watchdog::Enable();
    }
    else if (strcmp(cmd, "disable") == 0)
    {
        Watchdog::Disable();
    }
    else if (strcmp(cmd, "kick") == 0)
    {
        Watchdog::Kick();
    }
    else
    {
        Main.terminal.Puts("Unknown command");
    }
}

void WatchdogCommand(std::uint16_t argc, char* argv[])
{
    if (argc != 2)
    {
        Main.terminal.Puts("wdog int <enable|disable|kick>");
        return;
    }

    if (strcmp(argv[0], "int") == 0)
    {
        Handle<InternalWatchdog>(argv[1]);
    }
    else
    {
        Main.terminal.Puts("Unknown watchdog");
    }
}

void Hang(std::uint16_t argc, char* argv[])
{
    if (argc == 1 && strcmp(argv[0], "42") == 0)
    {
        Main.terminal.Puts(">");
        portENTER_CRITICAL();
        while (1)
            ;
    }
}