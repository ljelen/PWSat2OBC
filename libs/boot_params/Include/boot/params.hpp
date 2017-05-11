#ifndef LIBS_BOOT_PARAMS_INCLUDE_BOOT_PARAMS_HPP_
#define LIBS_BOOT_PARAMS_INCLUDE_BOOT_PARAMS_HPP_

#include <cstdint>

namespace boot
{
    constexpr std::uint32_t BootloaderMagicNumber = 0x55049196;

    enum class Reason
    {
        BootToUpper,
        SelectedIndex,
        CounterExpired,
        InvalidEntry,
        DownloadError,
        InvalidBootIndex
    };

    extern volatile std::uint32_t MagicNumber;
    extern volatile Reason BootReason;
    extern volatile std::uint8_t Index;

    /**
     * @brief Returns information whether boot parameters have been set.
     * @retval True The boot arguments have been set.
     * @retval False The boot arguments are not set.
     */
    bool IsBootInformationAvailable();
}

#endif /* LIBS_BOOT_PARAMS_INCLUDE_BOOT_PARAMS_HPP_ */