#ifndef ADCS_DETUMBLING_HPP_
#define ADCS_DETUMBLING_HPP_

#include <Eigen/Dense>
#include <array>
#include "interface_types.hpp"

namespace adcs
{
    /**
     * @defgroup adcs_detumbling  implementaiton of detumbling algorithm
     *
     * @{
     */

    /**
     *  @brief Implementation of detumbling  algorithm
     *
     * This procedure calculates the commanded magnetic dipole for the coils in
     * the detumbling mode based on the B-Dot control law. A high-pass filter for
     * the magnetic field time derivative is included.
     * */
    class Detumbling final
    {
      public:
        /**
         * @brief Set of detumbling algorithm  parameters
         */
        class DetumblingParameters final
        {
          public:
            /** @brief iteration time step
             * @unit [s]
             * @default 0.2
             */
            float dt = 0.2;

            /** @brief high-pass filter cut off frequency
             * @unit [rad/s]
             * @default 0.2
             */
            float wCutOff = 0.2;

            /** @brief B-dot gain
             * @unit [kg m^2 / s]
             * @default 2.879285e-5 * 1e15 -- unit conv - original gain * convwersion
             */
            float bDotGain = 2.879285e-5 * 1e15; // unit conv - original gain * convwersion

            /** @brief state of flags enabling coils
             * @unit [-]
             * @default {true, true, true}
             */
            std::array<bool, 3> coilsOn {{true, true, true}};
        };

        /**
         * @brief State of detumbling algorithm
         */
        class DetumblingState final
        {
          public:
            DetumblingState() = default;

            DetumblingState(const DetumblingParameters& p)
                : mtmDotPrev(Eigen::RowVector3f::Zero()), mtmMeasPrev(Eigen::RowVector3f::Zero()), params(DetumblingParameters(p))
            {
            }

            /** @brief Value of magnetic field derivative preserved from previous step   */
            Eigen::RowVector3f mtmDotPrev;

            /** @brief Value of magnetometer measurement preserved from previous step   */
            Eigen::RowVector3f mtmMeasPrev;

            /** @brief Set of algorithm parameters   */
            DetumblingParameters params;
        };

        Detumbling();

        /**
         * @brief Detumbling algorithm initialisation function
         *
         * This function sould be called before first step of algorithm
         * and  every time user intend to change parameters
         *
         * @param[out] state container
         * @param[in] parameters set
         * @return void
         */
        void initializeDetumbling(DetumblingState& state, const DetumblingParameters& param);

        /**
         * @brief Detumbling step function
         *
         * This function calculates value to be commanded to dipoles
         * based on magnetometer measurements and preserved state
         *
         * @param[out] values to be comanded to dipoles [1e-4 Am2]
         * @param[in]  magnetometer measurement [1e-7 T]
         * @param[in/out] state container
         * @return void
         */
        void stepDetumbling(DipoleVec& dipole, const MagVec& mgmt, DetumblingState& state);

      private:
        // field to store exp value calculated once on initialisation
        float mtmDotExp;
    };
    /** @} */
}

#endif /* ADCS_DETUMBLING_HPP_ */
