#ifndef LIBS_DRIVERS_EPS_INCLUDE_EPS_HK_HPP_
#define LIBS_DRIVERS_EPS_INCLUDE_EPS_HK_HPP_

#include "base/fwd.hpp"
#include "utils.h"

namespace devices
{
    namespace eps
    {
        namespace hk
        {
            /**
             * @defgroup eps_hk EPS Housekeeping
             * @ingroup eps
             *
             * @{
             */

            /**
             * @brief MPPT state
             */
            enum class MPPT_STATE : std::uint8_t
            {
                None = 0, //!< None
                A = 1,    //!< A
                B = 2,    //!< B
                C = 4,    //!< C
                D = 8,    //!< D
                E = 16,   //!< E
                F = 32    //!< F
            };

            /**
             * @brief DISTR LCL state
             */
            enum class DISTR_LCL_STATE : std::uint8_t
            {
                None = 0, //!< None
                A = 1,    //!< A
                B = 2,    //!< B
                C = 4,    //!< C
                D = 8,    //!< D
                E = 16,   //!< E
                F = 32,   //!< F
                G = 64,   //!< G
                H = 128   //!< H
            };

            /**
             * @brief DISTR LCL flags
             */
            enum class DISTR_LCL_FLAGB : std::uint8_t
            {
                None = 0, //!< None
                A = 1,    //!< A
                B = 2,    //!< B
                C = 4,    //!< C
                D = 8,    //!< D
                E = 16,   //!< E
                F = 32,   //!< F
                G = 64,   //!< G
                H = 128   //!< H
            };

            /**
             * @brief Battery controller state
             */
            enum class BATC_STATE : std::uint8_t
            {
                None = 0, //!< None
                A = 1,    //!< A
                B = 2,    //!< B
                C = 4,    //!< C
                D = 8,    //!< D
                E = 16,   //!< E
                F = 32,   //!< F
                G = 64,   //!< G
                H = 128   //!< H
            };

            /**
             * @brief Housekeeping of the 'other' controller
             */
            struct OtherControllerState
            {
                /**
                 * @brief Reads part of input stream
                 * @param reader Reader over input stream
                 */
                void ReadFrom(Reader& reader);

                void Write(BitWriter& writer);

                static constexpr std::uint32_t BitSize();

                /** @brief VOLT_3V3d, volts */
                uint10_t VOLT_3V3d;
            };

            constexpr std::uint32_t OtherControllerState::BitSize()
            {
                return Aggregate<decltype(VOLT_3V3d)>;
            }

            /**
             * @brief Housekeeping of the 'this' controller
             */
            struct ThisControllerState
            {
                ThisControllerState();

                /**
                 * @brief Reads part of input stream
                 * @param reader Reader over input stream
                 */
                void ReadFrom(Reader& reader);

                void Write(BitWriter& writer);

                static constexpr std::uint32_t BitSize();
                /** @brief Number of power cycles */
                std::uint16_t powerCycleCount;

                /** @brief Temperature, Celsius */
                uint10_t temperature;

                /** @brief Internal timer, seconds*/
                std::uint32_t uptime;

                /** @brief Error code */
                std::uint8_t errorCode;
            };

            constexpr std::uint32_t ThisControllerState::BitSize()
            {
                return Aggregate<decltype(powerCycleCount), decltype(temperature), decltype(uptime), decltype(errorCode)>;
            }

            /** @brief DCDC status*/
            struct DCDC_HK
            {
                /**
                 * @brief Reads part of input stream
                 * @param reader Reader over input stream
                 */
                void ReadFrom(Reader& reader);

                void Write(BitWriter& writer);

                static constexpr std::uint32_t BitSize();

                /** @brief DCDC: Temperature, Celsius*/
                uint10_t temperature;
            };

            constexpr std::uint32_t DCDC_HK::BitSize()
            {
                return Aggregate<decltype(temperature)>;
            }

            /** @brief DISTR status*/
            struct DISTR_HK
            {
                DISTR_HK();

                /**
                 * @brief Reads part of input stream
                 * @param reader Reader over input stream
                 */
                void ReadFrom(Reader& reader);

                void Write(BitWriter& writer);

                static constexpr std::uint32_t BitSize();

                /** @brief DISTR: temperature, Celsius */
                uint10_t Temperature;

                /** @brief DISTR: VOLT_3V3, volts*/
                uint10_t VOLT_3V3;

                /** @brief DIStr: CURR_3V3, mA*/
                uint10_t CURR_3V3;

                /** @brief DISTR: VOLT_5V, volts*/
                uint10_t VOLT_5V;

                /** @brief DISTR: CURR_5V, mA*/
                uint10_t CURR_5V;

                /** @brief DISTR: VOLT_VBAT, volts*/
                uint10_t VOLT_VBAT;

                /** @brief DISTR: CURR_VBAT, mA*/
                uint10_t CURR_VBAT;

                /** @brief DISTR: LCL states*/
                DISTR_LCL_STATE LCL_STATE;

                /** @brief DISTR: LCL FlagBs */
                DISTR_LCL_FLAGB LCL_FLAGB;
            };

            constexpr std::uint32_t DISTR_HK::BitSize()
            {
                return Aggregate<decltype(Temperature), //
                    decltype(VOLT_3V3),                 //
                    decltype(CURR_3V3),                 //
                    decltype(VOLT_5V),                  //
                    decltype(CURR_5V),                  //
                    decltype(VOLT_VBAT),                //
                    decltype(CURR_VBAT),                //
                    decltype(LCL_STATE),                //
                    decltype(LCL_FLAGB)>;
            }

            /** @brief MPPT status */
            struct MPPT_HK
            {
                MPPT_HK();

                /**
                 * @brief Reads part of input stream
                 * @param reader Reader over input stream
                 */
                void ReadFrom(Reader& reader);

                void Write(BitWriter& writer);

                static constexpr std::uint32_t BitSize();

                /** @brief MPPT: SOL_VOLT, volts*/
                uint12_t SOL_VOLT;
                /** @brief MPPT: SOL_CURR, mA*/
                uint12_t SOL_CURR;
                /** @brief MPPT: SOL_OUT_VOL, volts*/
                uint12_t SOL_OUT_VOLT;
                /** @brief MPPT: temperature, Celsius */
                uint12_t Temperature;
                /** @brief MPPT: algorithm state s*/
                MPPT_STATE MpptState;
            };

            constexpr std::uint32_t MPPT_HK::BitSize()
            {
                return Aggregate<decltype(SOL_VOLT), //
                    decltype(SOL_CURR),              //
                    decltype(SOL_OUT_VOLT),          //
                    decltype(Temperature),           //
                    decltype(MpptState)>;
            }

            /** @brief BATC status*/
            struct BATCPrimaryState
            {
                BATCPrimaryState();

                /**
                 * @brief Reads part of input stream
                 * @param reader Reader over input stream
                 */
                void ReadFrom(Reader& reader);

                void Write(BitWriter& writer);

                static constexpr std::uint32_t BitSize();

                /** @brief BATC: VOLT_A, volts*/
                uint10_t VOLT_A;
                /** @brief BATC: CHRG_CURR, mA*/
                uint10_t ChargeCurrent;
                /** @brief BATC: DCHRH_CURR, mA*/
                uint10_t DischargeCurrent;
                /** @brief BATC: temperature, Celsius */
                uint10_t Temperature;
                /** @brief BATC: internal states*/
                BATC_STATE State;
            };

            constexpr std::uint32_t BATCPrimaryState::BitSize()
            {
                return Aggregate<decltype(VOLT_A), //
                    decltype(ChargeCurrent),       //
                    decltype(DischargeCurrent),    //
                    decltype(Temperature),         //
                    decltype(State)>;
            }

            /** @brief Battery Pack status*/
            struct BatteryPackPrimaryState
            {
                /**
                 * @brief Reads part of input stream
                 * @param reader Reader over input stream
                 */
                void ReadFrom(Reader& reader);

                void Write(BitWriter& writer);

                static constexpr std::uint32_t BitSize();

                /** @brief Temp A, Celsius*/
                uint12_t temperatureA;

                /** @brief Temp B, Celsius*/
                uint12_t temperatureB;
            };

            constexpr std::uint32_t BatteryPackPrimaryState::BitSize()
            {
                return Aggregate<decltype(temperatureA), decltype(temperatureB)>;
            }

            /** @brief Battery Pack status*/
            struct BatteryPackSecondaryState
            {
                /**
                 * @brief Reads part of input stream
                 * @param reader Reader over input stream
                 */
                void ReadFrom(Reader& reader);

                void Write(BitWriter& writer);

                static constexpr std::uint32_t BitSize();

                /** @brief Battery Pack: Temp C, Celsius*/
                uint10_t temperatureC;
            };

            constexpr std::uint32_t BatteryPackSecondaryState::BitSize()
            {
                return Aggregate<decltype(temperatureC)>;
            }

            /** @brief Battery Controller status*/
            struct BATCSecondaryState
            {
                /**
                 * @brief Reads part of input stream
                 * @param reader Reader over input stream
                 */
                void ReadFrom(Reader& reader);

                void Write(BitWriter& writer);

                static constexpr std::uint32_t BitSize();

                /** @brief BATC: temp B, Celsius*/
                uint10_t voltB;
            };

            constexpr std::uint32_t BATCSecondaryState::BitSize()
            {
                return Aggregate<decltype(voltB)>;
            }

            /**
             * @brief Housekeeping of controller A
             * @telemetry_element
             */
            struct ControllerATelemetry
            {
                static const std::uint32_t Id = 6;

                /**
                 * @brief Reads part of input stream
                 * @param reader Reader over input stream
                 * @return true if read correctly
                 */
                bool ReadFrom(Reader& reader);

                /**
                 * @brief Write the system startup telemetry element to passed buffer writer object.
                 * @param[in] writer Buffer writer object that should be used to write the serialized state.
                 */
                void Write(BitWriter& writer);

                /**
                 * @brief Returns size of the serialized state in bytes.
                 * @return Size of the serialized state in bytes.
                 */
                static constexpr std::uint32_t BitSize();

                /** @brief MPPT_X */
                MPPT_HK mpptX;

                /** @brief MPPT_Y+*/
                MPPT_HK mpptYPlus;

                /** @brief MPPT_Y-*/
                MPPT_HK mpptYMinus;

                /** @brief DISTR status*/
                DISTR_HK distr;

                /** @brief BATC status*/
                BATCPrimaryState batc;

                /** @brief Battery Pack status*/
                BatteryPackPrimaryState bp;

                /** @brief Controller B status*/
                OtherControllerState other;

                /** @brief Controller A status*/
                ThisControllerState current;

                /** @brief DCDC: 3V3*/
                DCDC_HK dcdc3V3;

                /** @brief DCDC: 5V*/
                DCDC_HK dcdc5V;
            };

            constexpr std::uint32_t ControllerATelemetry::BitSize()
            {
                return decltype(mpptX)::BitSize() +   //
                    decltype(mpptYPlus)::BitSize() +  //
                    decltype(mpptYMinus)::BitSize() + //
                    decltype(distr)::BitSize() +      //
                    decltype(batc)::BitSize() +       //
                    decltype(bp)::BitSize() +         //
                    decltype(other)::BitSize() +      //
                    decltype(current)::BitSize() +    //
                    decltype(dcdc3V3)::BitSize() +    //
                    decltype(dcdc5V)::BitSize();
            }

            /**
             * @brief Housekeeping of controller B
             * @telemetry_element
             */
            struct ControllerBTelemetry
            {
                /**
                 * @brief Reads part of input stream
                 * @param reader Reader over input stream
                 * @return true if read correctly
                 */
                bool ReadFrom(Reader& reader);

                /**
                 * @brief Write the system startup telemetry element to passed buffer writer object.
                 * @param[in] writer Buffer writer object that should be used to write the serialized state.
                 */
                void Write(BitWriter& writer);

                /**
                 * @brief Returns size of the serialized state in bytes.
                 * @return Size of the serialized state in bytes.
                 */
                static constexpr std::uint32_t BitSize();

                /** @brief Battery Pack status*/
                BatteryPackSecondaryState bp;

                /** @brief Battery Controller status*/
                BATCSecondaryState batc;

                /** @brief Controller A status*/
                OtherControllerState other;

                /** @brief Controller B status*/
                ThisControllerState current;
            };

            constexpr std::uint32_t ControllerBTelemetry::BitSize()
            {
                return decltype(bp)::BitSize() + //
                    decltype(batc)::BitSize() +  //
                    decltype(other)::BitSize() + //
                    decltype(current)::BitSize();
            }

            /** @} */
        }
    }
}

#endif /* LIBS_DRIVERS_EPS_INCLUDE_EPS_HK_HPP_ */
