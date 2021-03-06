#include <cstdint>
#include <cstring>
#include <tuple>
#include "antenna/driver.h"
#include "antenna/telemetry.hpp"
#include "antenna_task.hpp"
#include "gsl/gsl_util"
#include "logger/logger.h"
#include "mission/base.hpp"
#include "mission/obc.hpp"
#include "power/power.h"
#include "system.h"
#include "time/TimePoint.h"

using namespace std::chrono_literals;

namespace mission
{
    namespace antenna
    {
        template <typename... Elements, std::size_t... Indexes, typename Element = std::tuple_element_t<0, std::tuple<Elements...>>>
        static constexpr std::array<Element, sizeof...(Elements)> ToArray(
            std::tuple<Elements...> t, std::integer_sequence<std::size_t, Indexes...> /*i*/)
        {
            return {std::get<Indexes>(t)...};
        }

        template <typename... Tuples> static decltype(auto) Join(const Tuples... tuples)
        {
            auto joined = std::tuple_cat(tuples...);

            constexpr auto TupleSize = std::tuple_size<decltype(joined)>::value;

            return ToArray(joined, std::make_index_sequence<TupleSize>());
        }

        /** @brief Helper class for building deployment steps */
        template <AntennaChannel Channel> struct Step
        {
            /** @brief Power on controller */
            static constexpr AntennaTask::StepDescriptor PowerOn = {AntennaTask::PowerOn, Channel, AntennaId::ANTENNA_AUTO_ID, 0s, 9s};

            /** @brief Reset controller */
            static constexpr AntennaTask::StepDescriptor Reset = {AntennaTask::Reset, Channel, AntennaId::ANTENNA_AUTO_ID, 0s, 59s};

            /** @brief Arm controller */
            static constexpr AntennaTask::StepDescriptor Arm = {AntennaTask::Arm, Channel, AntennaId::ANTENNA_AUTO_ID, 0s, 59s};

            /** @brief Perform auto deployment */
            static constexpr AntennaTask::StepDescriptor AutoDeploy = {
                AntennaTask::Deploy, Channel, AntennaId::ANTENNA_AUTO_ID, 4 * 30s, 179s};

            /** @brief Perform manual deployment */
            template <AntennaId Antenna>
            static constexpr AntennaTask::StepDescriptor ManualDeploy = {AntennaTask::Deploy, Channel, Antenna, 30s, 89s};

            /** @brief Disarm controller */
            static constexpr AntennaTask::StepDescriptor Disarm = {AntennaTask::Disarm, Channel, AntennaId::ANTENNA_AUTO_ID, 0s, 0s};

            /** @brief Power off controller */
            static constexpr AntennaTask::StepDescriptor PowerOff = {AntennaTask::PowerOff, Channel, AntennaId::ANTENNA_AUTO_ID, 0s, 119s};

            /** @brief Perform full sequence of auto deployment */
            static constexpr auto FullSequenceAuto = std::make_tuple(PowerOn, Reset, Arm, AutoDeploy, Disarm, PowerOff);

            /** @brief Perform full sequence of manual deployment on single antenna */
            template <AntennaId Antenna>
            static constexpr auto FullSequenceManual = std::make_tuple(PowerOn, Reset, Arm, ManualDeploy<Antenna>, Disarm, PowerOff);
        };

        std::array<AntennaTask::StepDescriptor, 60> AntennaTask::Steps = Join( //
            Step<AntennaChannel::ANTENNA_PRIMARY_CHANNEL>::FullSequenceAuto,
            Step<AntennaChannel::ANTENNA_PRIMARY_CHANNEL>::FullSequenceManual<AntennaId::ANTENNA1_ID>,
            Step<AntennaChannel::ANTENNA_PRIMARY_CHANNEL>::FullSequenceManual<AntennaId::ANTENNA2_ID>,
            Step<AntennaChannel::ANTENNA_PRIMARY_CHANNEL>::FullSequenceManual<AntennaId::ANTENNA3_ID>,
            Step<AntennaChannel::ANTENNA_PRIMARY_CHANNEL>::FullSequenceManual<AntennaId::ANTENNA4_ID>,
            Step<AntennaChannel::ANTENNA_BACKUP_CHANNEL>::FullSequenceAuto,
            Step<AntennaChannel::ANTENNA_BACKUP_CHANNEL>::FullSequenceManual<AntennaId::ANTENNA1_ID>,
            Step<AntennaChannel::ANTENNA_BACKUP_CHANNEL>::FullSequenceManual<AntennaId::ANTENNA2_ID>,
            Step<AntennaChannel::ANTENNA_BACKUP_CHANNEL>::FullSequenceManual<AntennaId::ANTENNA3_ID>,
            Step<AntennaChannel::ANTENNA_BACKUP_CHANNEL>::FullSequenceManual<AntennaId::ANTENNA4_ID>);

        AntennaTask::AntennaTask(std::tuple<IAntennaDriver&, services::power::IPowerControl&> args)
            : _powerControl(std::get<services::power::IPowerControl&>(args)), //
              _antenna(std::get<IAntennaDriver&>(args)),                      //
              _step(0),                                                       //
              _nextStepAt(0),                                                 //
              _retryCounter(StepRetries),                                     //
              _sync(nullptr), _controllerPoweredOn(false)
        {
        }

        bool AntennaTask::Initialize()
        {
            this->_sync = System::CreateBinarySemaphore(1);
            System::GiveSemaphore(this->_sync);
            return true;
        }

        bool AntennaTask::IsDeploymentDisabled(const SystemState& state)
        {
            state::AntennaConfiguration cfg;
            return state.PersistentState.Get(cfg) && cfg.IsDeploymentDisabled();
        }

        bool AntennaTask::Condition(const SystemState& state, void* param)
        {
            auto This = reinterpret_cast<AntennaTask*>(param);

            if (!mission::IsInitialSilentPeriodFinished(state.Time))
            {
                return false;
            }

            if (This->_step >= Steps.size())
            {
                return false;
            }

            if (state.Time < This->_nextStepAt)
            {
                return false;
            }

            return !This->IsDeploymentDisabled(state);
        }

        void AntennaTask::Action(SystemState& state, void* param)
        {
            UNREFERENCED_PARAMETER(state);
            auto This = reinterpret_cast<AntennaTask*>(param);

            LOGF(LOG_LEVEL_INFO, "[ant] Performing step %d", This->_step);

            auto stepDescriptor = Steps[This->_step];

            auto result = stepDescriptor.Action(This, stepDescriptor.Channel, stepDescriptor.Antenna, stepDescriptor.burnTime);

            if (OS_RESULT_FAILED(result) && This->_retryCounter > 1)
            {
                LOGF(LOG_LEVEL_WARNING, "[ant] Step %d failed. Will retry %d times more", This->_step, This->_retryCounter);
                This->_retryCounter--;
                return;
            }

            This->_step++;
            This->_nextStepAt = state.Time + stepDescriptor.waitTime;
            This->_retryCounter = StepRetries;

            if (This->_step >= Steps.size())
            {
                state.AntennaState.SetDeployment(true);
            }
        }

        ActionDescriptor<SystemState> AntennaTask::BuildAction()
        {
            ActionDescriptor<SystemState> descriptor;
            descriptor.name = "Deploy Antenna Action";
            descriptor.param = this;
            descriptor.condition = Condition;
            descriptor.actionProc = Action;
            return descriptor;
        }

        UpdateResult AntennaTask::Update(SystemState& state, void* param)
        {
            auto This = reinterpret_cast<AntennaTask*>(param);

            if (!state.AntennaState.IsDeployed() && This->IsDeploymentDisabled(state))
            {
                state.AntennaState.SetDeployment(true);
            }

            if (!This->_controllerPoweredOn)
            {
                return UpdateResult::Ok;
            }

            Lock l(This->_sync, 200ms);

            if (!l())
            {
                return UpdateResult::Warning;
            }

            This->_antenna.GetTelemetry(This->_currentTelemetry);

            return UpdateResult::Ok;
        }

        UpdateDescriptor<SystemState> AntennaTask::BuildUpdate()
        {
            UpdateDescriptor<SystemState> descriptor;
            descriptor.name = "Deploy Antenna Update";
            descriptor.param = this;
            descriptor.updateProc = Update;
            return descriptor;
        }

        bool AntennaTask::GetTelemetry(devices::antenna::AntennaTelemetry& telemetry) const
        {
            Lock l(this->_sync, 100ms);

            if (!l())
            {
                return false;
            }

            telemetry = this->_currentTelemetry;

            return true;
        }

        OSResult AntennaTask::PowerOn(
            AntennaTask* task, AntennaChannel channel, AntennaId /*antenna*/, std::chrono::milliseconds /*burnTime*/)
        {
            bool r;
            if (channel == AntennaChannel::ANTENNA_PRIMARY_CHANNEL)
            {
                r = task->_powerControl.PrimaryAntennaPower(true);
            }
            else
            {
                r = task->_powerControl.BackupAntennaPower(true);
            }

            if (r)
            {
                task->_controllerPoweredOn = true;
            }

            return r ? OSResult::Success : OSResult::DeviceNotFound;
        }

        OSResult AntennaTask::Reset(
            AntennaTask* task, AntennaChannel channel, AntennaId /*antenna*/, std::chrono::milliseconds /*burnTime*/)
        {
            return task->_antenna.Reset(channel);
        }

        OSResult AntennaTask::Arm(AntennaTask* task, AntennaChannel channel, AntennaId /*antenna*/, std::chrono::milliseconds /*burnTime*/)
        {
            return task->_antenna.Arm(channel);
        }

        OSResult AntennaTask::Deploy(AntennaTask* task, AntennaChannel channel, AntennaId antenna, std::chrono::milliseconds burnTime)
        {
            return task->_antenna.DeployAntenna(channel, antenna, burnTime, true);
        }

        OSResult AntennaTask::Disarm(
            AntennaTask* task, AntennaChannel channel, AntennaId /*antenna*/, std::chrono::milliseconds /*burnTime*/)
        {
            return task->_antenna.Disarm(channel);
        }

        OSResult AntennaTask::PowerOff(
            AntennaTask* task, AntennaChannel channel, AntennaId /*antenna*/, std::chrono::milliseconds /*burnTime*/)
        {
            bool r;
            if (channel == AntennaChannel::ANTENNA_PRIMARY_CHANNEL)
            {
                r = task->_powerControl.PrimaryAntennaPower(false);
            }
            else
            {
                r = task->_powerControl.BackupAntennaPower(false);
            }

            if (r)
            {
                task->_controllerPoweredOn = false;
            }

            return r ? OSResult::Success : OSResult::DeviceNotFound;
        }
        /** @}*/
    }
}
