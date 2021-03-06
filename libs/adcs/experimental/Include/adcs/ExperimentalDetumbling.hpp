#ifndef LIBS_ADCS_EXPERIMENTAL_ADCS_EXPERIMENTAL_DETUMBLING_HPP
#define LIBS_ADCS_EXPERIMENTAL_ADCS_EXPERIMENTAL_DETUMBLING_HPP

#include "DetumblingComputations.hpp"
#include "adcs/adcs.hpp"
#include "base/hertz.hpp"
#include "imtq/imtq.h"
#include "power/fwd.hpp"

namespace adcs
{
    /**
     * @brief Experimental detumbling.
     */
    class ExperimentalDetumbling final : public IAdcsProcessor
    {
      public:
        /**
         * @brief Ctor.
         *
         * @param[in] imtqDriver_ Low level imtq module driver.
         * @param[in] powerControl_ Power control interface
         */
        ExperimentalDetumbling(devices::imtq::IImtqDriver& imtqDriver_, services::power::IPowerControl& powerControl_);

        /**
         * @brief Sets built-in or alternative self-test algorithm.
         *
         * @param[in] enable Whether to enable the alternative algorithm.
         */
        void SetTryFixIsisErrors(bool enable);

        virtual OSResult Initialize() override final;

        virtual OSResult Enable() override final;

        virtual OSResult Disable() override final;

        virtual void Process() override final;

        virtual std::chrono::milliseconds GetWait() const override final;

        /** @brief Algorithm refresh frequency. */
        static constexpr chrono_extensions::hertz Frequency = chrono_extensions::hertz{1.0 / DetumblingComputations::Parameters::dt};

        /** @brief Coil actuation timeout. */
        static constexpr std::chrono::milliseconds ActuationTimeout = std::chrono::milliseconds(500);

      private:
        OSResult PerformSelfTest();

        /** @brief Detumbling computations algorithm. */
        DetumblingComputations detumblingComputations;

        /** @brief Detumbling algorithm state. */
        DetumblingComputations::State detumblingState;

        /** @brief Low level imtq module driver. */
        devices::imtq::IImtqDriver& imtqDriver;
        services::power::IPowerControl& powerControl;

        /** @brief Semaphore for tasks synchronization.*/
        OSSemaphoreHandle syncSemaphore;

        /** @brief Whether to enable the alternative self-test algorithm. */
        bool tryToFixIsisErrors;
    };
}

#endif /* LIBS_ADCS_EXPERIMENTAL_ADCS_EXPERIMENTAL_DETUMBLING_HPP */
