#ifndef LIBS_DRIVERS_IMTQ_INCLUDE_IMTQ_IIMTQ_DRIVER_HPP
#define LIBS_DRIVERS_IMTQ_INCLUDE_IMTQ_IIMTQ_DRIVER_HPP

#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include "fwd.hpp"
#include "gsl/span"

namespace devices
{
    namespace imtq
    {
        /**
         * @brief Array of 3 elements.
         */
        template <typename T> using Vector3 = std::array<T, 3>;

        /**
         * @brief Structure containing Three-axis magnetometer measurement.
         */
        struct MagnetometerMeasurementResult
        {
            /**
             * @brief Magnetic field measurement.
             */
            Vector3<MagnetometerMeasurement> data;

            /**
             * @brief true if coil were enabled during measurement
             */
            bool coilActuationDuringMeasurement;
        };

        /**
         * @brief Imtq driver error, used to determine why command have failed.
         */
        enum class ImtqDriverError
        {
            /**
             * @brief No error
             */
            OK,

            /**
             * @brief I2C Write operation failed during command execution
             */
            I2CWriteFailed,

            /**
             * @brief I2C Read operation failed during command execution
             */
            I2CReadFailed,

            /**
             * @brief Imtq returned wrong opcode in response
             */
            WrongOpcodeInResponse,

            /**
             * @brief Error in Status byte returned by Imtq.
             * Further investigation should be done by sending
             * ImtqDriver::LastStatus field to the operator.
             */
            StatusError,
        };

        /**
         * @brief Imtq internal error structure used during self-test.
         */
        class Error
        {
          public:
            /**
             * @brief Default constructor (no-error).
             */
            constexpr Error() : value{0}
            {
            }

            /**
             * @brief Constructor from std::uint8_t (as returned from Imtq hardware) (TC-DR-07).
             * @param[in] val Error value, defined in TC-DR-07.
             */
            constexpr Error(std::uint8_t val) : value{val}
            {
            }

            /**
             * @brief Returns true if no error was detected.
             */
            bool Ok() const;

            /**
             * @brief Returns error code as defined in User Manual (TC-DR-07)
             */
            std::uint8_t GetValue() const;

          private:
            std::uint8_t value;
        };

        /**
         * @brief Imtq internal state structure used during GetSystemState command.
         */
        struct State
        {
            /**
             * @brief ctor.
             */
            constexpr State();

            /**
             * @brief Current status;
             */
            std::uint8_t status;

            /**
             * @brief Imtq mode
             */
            Mode mode;

            /**
             * @brief Internal error occured during imtq operations
             */
            Error error;

            /**
             * @brief True if any configuration parameter was updated since start-up (via SetParameter command)
             */
            bool anyParameterUpdatedSinceStartup;

            /**
             * @brief Imtq uptime, with one second resolution
             */
            std::chrono::seconds uptime;
        };

        constexpr State::State() : status{0}, mode{Mode::Idle}, error{0}, anyParameterUpdatedSinceStartup{false}, uptime{0}
        {
        }

        /**
         * @brief SelfTestResult structure returned during GetSelfTestResult.
         */
        struct SelfTestResult
        {
            /**
             * @brief Self test steps, each step should be performed (one after another in table) [TC-DR-07]
             */
            enum class Step
            {
                Init = 0x00, //
                Xp = 0x01,
                Xn = 0x02,
                Yp = 0x03,
                Yn = 0x04,
                Zp = 0x05,
                Zn = 0x06,
                Fina = 0x07
            };

            /**
             * @brief Each of self-test steps result is defined by following structure.
             */
            struct StepResult
            {
                /**
                 * @brief Actual step during self-test. Should be unique and increasing for each element.
                 */
                Step actualStep;

                /**
                 * @brief Error occurred during this step
                 */
                Error error;

                /**
                 * @brief Three-axis raw magnetometer data during self-test actuation.
                 */
                Vector3<MagnetometerMeasurement> RawMagnetometerMeasurement;

                /**
                 * @brief Three-axis calibrated magnetometer data during self-test actuation.
                 */
                Vector3<MagnetometerMeasurement> CalibratedMagnetometerMeasurement;

                /**
                 * @brief Three-axis coil current during self-test actuation.
                 */
                Vector3<Current> CoilCurrent;

                /**
                 * @brief Three-axis coil temperature during self-test actuation.
                 */
                Vector3<TemperatureMeasurement> CoilTemperature;
            };

            /**
             * @brief Array containing eight self-test steps.
             */
            std::array<StepResult, 8> stepResults;
        };

        /**
         * @brief Structure containing detumbling data (updated internally each iteration - nominal 1 Hz).
         */
        struct DetumbleData
        {
            /**
             * @brief Three-axis calibrated magnetometer data during actual detumbling step.
             */
            Vector3<MagnetometerMeasurement> calibratedMagnetometerMeasurement;

            /**
             * @brief Three-axis filtered magnetometer data during actual detumbling step.
             */
            Vector3<MagnetometerMeasurement> filteredMagnetometerMeasurement;

            /**
             * @brief Three-axis BDot algorithm internal value during self-test actuation.
             */
            Vector3<BDotType> bDotData;

            /**
             * @brief Three-axis commanded dipole during self-test actuation.
             */
            Vector3<Dipole> commandedDipole;

            /**
             * @brief Three-axis commanded coil current during self-test actuation.
             */
            Vector3<Current> commandedCurrent;

            /**
             * @brief Three-axis measured coil current during self-test actuation.
             */
            Vector3<Current> measuredCurrent;
        };

        /**
         * @brief RAW housekeeping data structure. Conversion formulas are in TC-DR-09.
         */
        struct HouseKeepingRAW
        {
            /**
             * @brief Digital supply voltage.
             */
            std::uint16_t digitalVoltage;

            /**
              * @brief Analog supply voltage.
              */
            std::uint16_t analogVoltage;

            /**
             * @brief Digital supply current.
             */
            std::uint16_t digitalCurrent;

            /**
             * @brief Analog supply current.
             */
            std::uint16_t analogCurrent;

            /**
             * @brief Three-axis coil current measurement
             */
            Vector3<std::uint16_t> coilCurrent;

            /**
             * @brief Three-axis coil temperature measurement
             */
            Vector3<std::uint16_t> coilTemperature;

            /**
             * @brief Measured microcontroller temperature.
             */
            std::uint16_t MCUtemperature;

            /**
             * @brief Current status;
             */
            std::uint8_t status;
        };

        /**
         * @brief Engineering notation housekeeping data structure. Conversion formulas are in TC-DR-09.
         */
        struct HouseKeepingEngineering
        {
            /**
             * @brief Digital supply voltage.
             */
            VoltageInMiliVolt digitalVoltage;

            /**
             * @brief Analog supply voltage.
             */
            VoltageInMiliVolt analogVoltage;

            /**
             * @brief Digital supply current.
             */
            Current digitalCurrent;

            /**
             * @brief Analog supply current.
             */
            Current analogCurrent;

            /**
             * @brief Three-axis coil current measurement
             */
            Vector3<Current> coilCurrent;

            /**
             * @brief Three-axis coil temperature measurement
             */
            Vector3<TemperatureMeasurement> coilTemperature;

            /**
             * @brief Measured microcontroller temperature.
             */
            TemperatureMeasurement MCUtemperature;

            /**
             * @brief Current status;
             */
            std::uint8_t status;
        };

        /**
         * @brief Interface for Imtq commanding.
         */
        struct IImtqDriver
        {
            /**
             * Performs self test
             * @param[out] result Self test result
             * @param[in] tryToFixIsisErrors Whether to enable the alternative self-test algorithm.
             * @return Operation status.
             * This method runs:
             * 1) starts all axis self-test (TC-OP-08)
             * 2) waits specified amount of time for command to complete
             * 3) reads self test result (TC-DR-07)
             */
            virtual bool PerformSelfTest(SelfTestResult& result, bool tryToFixIsisErrors) = 0;

            /**
             * Measures magnetometer, having in mind possible on-going actuation.
             * @param[out] result Three axis magnetometer measurement
             * @return Operation status.
             * This method runs:
             * 1) cancels on-going actuation (TC-OP-03)
             * 2) waits wait magnetic field decay
             * 3) sends magnetometer measurement request (TC-OP-04)
             * 4) waits until read finishes
             * 3) reads magnetometer data (TC-DR-03)
             */
            virtual bool MeasureMagnetometer(Vector3<MagnetometerMeasurement>& result) = 0;

            // ----- Commands -----

            /**
             * @brief Performs SoftwareReset (TC-OP-01)
             * @return Operation status.
             */
            virtual bool SoftwareReset() = 0;

            /**
             * @brief Sends NoOperation command (TC-OP-02)
             * @return Operation status.
             */
            virtual bool SendNoOperation() = 0;

            /**
             * @brief Cancels current imtq operation (detumbling/self-test) (TC-OP-03)
             * @return Operation status.
             */
            virtual bool CancelOperation() = 0;

            /**
             * @brief Initiates magnetometer measurement (TC-OP-04)
             * @return Operation status.
             */
            virtual bool StartMTMMeasurement() = 0;

            /**
             * @brief Starts current-based actuation for specified time (TC-OP-05)
             * @param[in] current Three-axis current values for X, Y and Z axes.
             * @param[in] duration Duration of actuation, after which it will be cancelled automatically. 1ms resolution, maximum time 65
             * seconds.
             * @return Operation status.
             */
            virtual bool StartActuationCurrent(const Vector3<Current>& current, std::chrono::milliseconds duration) = 0;

            /**
             * @brief Starts dipole-based actuation for specified time (TC-OP-06)
             * @param[in] dipole Three-axis dipole values for X, Y and Z axes.
             * @param[in] duration Duration of actuation, after which it will be cancelled automatically. 1ms resolution, maximum time 65
             * seconds.
             * @return Operation status.
             */
            virtual bool StartActuationDipole(Vector3<Dipole> dipole, std::chrono::milliseconds duration) = 0;

            /**
             * @brief Starts self test (TC-OP-08)
             * @return Operation status.
             */
            virtual bool StartAllAxisSelfTest() = 0;

            /**
             * @brief Starts internal detumbling (TC-OP-09)
             * @param[in] duration Duration of detumbling. Resolution 1 second, maximum duration 65535 seconds.
             * @return Operation status.
             */
            virtual bool StartBDotDetumbling(std::chrono::seconds duration) = 0;

            // ----- Data requests -----
            /**
             * @brief Reads imtq State (TC-DR-01)
             * @param[out] state State structure read from imtq
             * @return Operation status.
             */
            virtual bool GetSystemState(State& state) = 0;

            /**
             * @brief Reads calibrated magnetometer data (TC-DR-03)
             * @param[out] result Magnetometer measurement
             * @return Operation status.
             * This command should be used after StartMTMMeasurement, having in mind magnetometer integration time.
             */
            virtual bool GetCalibratedMagnetometerData(MagnetometerMeasurementResult& result) = 0;

            /**
             * @brief Reads coil current (TC-DR-04)
             * @param[out] result Three-axis coil current measurement
             * @return Operation status.
             * This value is updated internally each 1ms.
             */
            virtual bool GetCoilCurrent(Vector3<Current>& result) = 0;

            /**
             * @brief Reads coil temperature (TC-DR-05)
             * @param[out] result Three-axis coil temperature measurement
             * @return Operation status.
             * This value is updated internally each 1ms.
             */
            virtual bool GetCoilTemperature(Vector3<TemperatureMeasurement>& result) = 0;

            /**
             * @brief Reads self-test result (TC-DR-07)
             * @param[out] result Self-test result
             * @return Operation status.
             * This command should be used after StartAllAxisSelfTest, having in mind self-test time.
             */
            virtual bool GetSelfTestResult(SelfTestResult& result) = 0;

            /**
             * @brief Reads imtq detumbling state (TC-DR-08)
             * @param[out] result Detumble data structure.
             * @return Operation status.
             * This value is updated internally each 1s.
             */
            virtual bool GetDetumbleData(DetumbleData& result) = 0;

            /**
             * @brief Reads HouseKeeping - RAW type (TC-DR-09)
             * @param[out] result RAW HouseKeeping read from imtq
             * @return Operation status.
             * This value is updated internally each 1ms.
             */
            virtual bool GetHouseKeepingRAW(HouseKeepingRAW& result) = 0;

            /**
             * @brief Reads HouseKeeping - Engineering type (TC-DR-10)
             * @param[out] result Engineering HouseKeeping read from imtq
             * @return Operation status.
             * This value is updated internally each 1ms.
             */
            virtual bool GetHouseKeepingEngineering(HouseKeepingEngineering& result) = 0;

            // ----- Configuration -----
            /**
             * @brief Parameter alias type
             */
            using Parameter = std::uint16_t;

            /**
             * @brief Reads specified parameter from imtq memory (TC-CF-01)
             * @param[in] id Parameter id
             * @param[out] result Parameter value. Should be span, with correct length, to memory to write result to.
             * @return Operation status.
             */
            virtual bool GetParameter(Parameter id, gsl::span<std::uint8_t> result) = 0;

            /**
             * @brief Sets specified parameter in imtq memory (TC-CF-02)
             * @param[in] id Parameter id
             * @param[in] value Parameter value. Should be span, with correct length, to memory containing parameter value.
             * @return Operation status.
             */
            virtual bool SetParameter(Parameter id, gsl::span<const std::uint8_t> value) = 0;

            /**
             * @brief Resets specified parameter in imtq memory to default one and reads it (TC-CF-03)
             * @param[in] id Parameter id
             * @param[out] result Parameter value. Should be span, with correct length, to memory to write result to.
             * @return Operation status.
             */
            virtual bool ResetParameterAndGetDefault(Parameter id, gsl::span<std::uint8_t> result) = 0;
        };
    }
}

#endif
