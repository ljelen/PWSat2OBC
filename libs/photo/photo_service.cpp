#include "photo_service.hpp"
#include <array>
#include "fs/fs.h"
#include "logger/logger.h"
#include "power/power.h"

using namespace std::chrono_literals;

namespace services
{
    namespace photo
    {
        static std::array<std::uint8_t, 512_KB> PhotoBuffer;

        BufferInfo::BufferInfo() : _status(BufferStatus::Empty)
        {
        }

        BufferInfo::BufferInfo(BufferStatus status, gsl::span<std::uint8_t> buffer) : _status(status), _buffer(buffer)
        {
        }

        PhotoService::PhotoService(
            services::power::IPowerControl& power, ICamera& camera, ICameraSelector& selector, services::fs::IFileSystem& fileSystem)
            : _power(power), _camera(camera), _selector(selector), _fileSystem(fileSystem), _freeSpace(PhotoBuffer.begin()),
              _task("Photos", this, TaskProc)
        {
        }

        OSResult PhotoService::Invoke(DisableCamera command)
        {
            bool result = false;

            switch (command.Which())
            {
                case Camera::Nadir:
                    result = this->_power.CameraNadir(false);
                    break;
                case Camera::Wing:
                    result = this->_power.CameraWing(false);
                    break;
            }

            return result ? OSResult::Success : OSResult::IOError;
        }

        OSResult PhotoService::Invoke(EnableCamera command)
        {
            bool result = false;

            switch (command.Which())
            {
                case Camera::Nadir:
                    result = this->_power.CameraNadir(true);
                    break;
                case Camera::Wing:
                    result = this->_power.CameraWing(true);
                    break;
            }

            if (!result)
            {
                return OSResult::PowerFailure;
            }

            this->_selector.Select(command.Which());

            System::SleepTask(3s);

            auto syncResult = this->_camera.Sync();

            if (!syncResult.Successful)
            {
                return OSResult::DeviceNotFound;
            }

            return OSResult::Success;
        }

        OSResult PhotoService::Invoke(TakePhoto command)
        {
            this->_selector.Select(command.Which());

            for (auto i = 0; i < 3; i++)
            {
                auto r = this->_camera.TakePhoto(command.Resolution());

                if (r == TakePhotoResult::Success)
                {
                    return OSResult::Success;
                }

                Invoke(DisableCamera(command.Which()));
                Invoke(EnableCamera(command.Which()));
            }

            return OSResult::DeviceNotFound;
        }

        OSResult PhotoService::Invoke(DownloadPhoto command)
        {
            if (command.BufferId() >= BuffersCount)
            {
                return OSResult::InvalidArgument;
            }

            this->_selector.Select(command.Which());

            {
                Lock l(this->_sync, InfiniteTimeout);
                this->_bufferInfos[command.BufferId()] = BufferInfo(BufferStatus::Downloading, 0);
            }

            DownloadPhotoResult r(OSResult::DeviceNotFound);

            for (auto i = 0; i < 3; i++)
            {
                r = this->_camera.DownloadPhoto(gsl::make_span(this->_freeSpace, PhotoBuffer.end()));

                if (r.IsSuccess())
                    break;

                LOGF(LOG_LEVEL_WARNING, "[photo] Retrying (%d) download from %d", i, num(command.Which()));
            }
            if (r.IsSuccess())
            {
                Lock l(this->_sync, InfiniteTimeout);
                this->_bufferInfos[command.BufferId()] = BufferInfo(BufferStatus::Occupied, r.Success());
                this->_freeSpace += r.Success().size();
                return OSResult::Success;
            }

            {
                Lock l(this->_sync, InfiniteTimeout);
                this->_bufferInfos[command.BufferId()] = BufferInfo(BufferStatus::Failed, 0);
            }

            return r.Error();
        }

        OSResult PhotoService::Invoke(Reset /*command*/)
        {
            this->_freeSpace = PhotoBuffer.begin();
            Lock l(this->_sync, InfiniteTimeout);
            std::fill(this->_bufferInfos.begin(), this->_bufferInfos.end(), BufferInfo());
            return OSResult::Success;
        }

        OSResult PhotoService::Invoke(SavePhoto command)
        {
            services::fs::File f(
                this->_fileSystem, command.Path(), services::fs::FileOpen::CreateAlways, services::fs::FileAccess::WriteOnly);

            if (!f)
            {
                return OSResult::IOError;
            }

            if (command.BufferId() >= BuffersCount)
            {
                return OSResult::InvalidArgument;
            }

            auto buffer = this->GetBufferInfo(command.BufferId());
            LOGF(LOG_LEVEL_DEBUG,
                "[photo] Saving photo from buffer %d to %s (status: %d, size: %d bytes))",
                command.BufferId(),
                command.Path(),
                num(buffer.Status()),
                buffer.Size());

            if (buffer.Status() == BufferStatus::Empty)
            {
                const char* marker = "Empty";
                f.Write(gsl::make_span(reinterpret_cast<const std::uint8_t*>(marker), 6));
                return OSResult::BufferNotAvailable;
            }

            if (buffer.Status() == BufferStatus::Failed)
            {
                const char* marker = "Failed";
                f.Write(gsl::make_span(reinterpret_cast<const std::uint8_t*>(marker), 7));
                return OSResult::BufferNotAvailable;
            }

            f.Write(buffer.Buffer());

            return OSResult::Success;
        }

        OSResult PhotoService::Invoke(Sleep command)
        {
            System::SleepTask(command.Duration());
            return OSResult::Success;
        }

        BufferInfo PhotoService::GetBufferInfo(std::uint8_t bufferId) const
        {
            if (bufferId >= BuffersCount)
            {
                return BufferInfo{};
            }

            BufferInfo tmp;
            {
                Lock l(this->_sync, InfiniteTimeout);
                tmp = this->_bufferInfos[bufferId];
            }
            return tmp;
        }

        bool PhotoService::IsEmpty(std::uint8_t bufferId) const
        {
            Lock l(this->_sync, InfiniteTimeout);
            return bufferId < BuffersCount && this->_bufferInfos[bufferId].Status() == BufferStatus::Empty;
        }

        void PhotoService::Initialize()
        {
            this->_commandQueue.Create();
            this->_sync = System::CreateBinarySemaphore();
            System::GiveSemaphore(this->_sync);
            this->_flags.Initialize();
            this->_task.Create();
        }

        void PhotoService::Schedule(DisableCamera command)
        {
            PossibleCommand cmd;
            cmd.DisableCameraCommand = command;
            cmd.Selected = Command::DisableCamera;
            this->_commandQueue.Push(cmd, InfiniteTimeout);
            this->_flags.Clear(IdleFlag);
        }
        void PhotoService::Schedule(EnableCamera command)
        {
            PossibleCommand cmd;
            cmd.EnableCameraCommand = command;
            cmd.Selected = Command::EnableCamera;
            this->_commandQueue.Push(cmd, InfiniteTimeout);
            this->_flags.Clear(IdleFlag);
        }
        void PhotoService::Schedule(TakePhoto command)
        {
            PossibleCommand cmd;
            cmd.TakePhotoCommand = command;
            cmd.Selected = Command::TakePhoto;
            this->_commandQueue.Push(cmd, InfiniteTimeout);
            this->_flags.Clear(IdleFlag);
        }
        void PhotoService::Schedule(DownloadPhoto command)
        {
            PossibleCommand cmd;
            cmd.DownloadPhotoCommand = command;
            cmd.Selected = Command::DownloadPhoto;
            this->_commandQueue.Push(cmd, InfiniteTimeout);
            this->_flags.Clear(IdleFlag);
        }
        void PhotoService::Schedule(Reset command)
        {
            PossibleCommand cmd;
            cmd.ResetCommand = command;
            cmd.Selected = Command::Reset;
            this->_commandQueue.Push(cmd, InfiniteTimeout);
            this->_flags.Clear(IdleFlag);
        }
        void PhotoService::Schedule(SavePhoto command)
        {
            PossibleCommand cmd;
            cmd.SavePhotoCommand = command;
            cmd.Selected = Command::SavePhoto;
            this->_commandQueue.Push(cmd, InfiniteTimeout);
            this->_flags.Clear(IdleFlag);
        }

        void PhotoService::Schedule(Sleep command)
        {
            PossibleCommand cmd;
            cmd.SleepCommand = command;
            cmd.Selected = Command::Sleep;
            this->_commandQueue.Push(cmd, InfiniteTimeout);
            this->_flags.Clear(IdleFlag);
        }

        bool PhotoService::WaitForFinish(std::chrono::milliseconds timeout)
        {
            auto r = this->_flags.WaitAny(IdleFlag, false, timeout);

            return has_flag(r, IdleFlag);
        }

        void PhotoService::TaskProc(PhotoService* This)
        {
            LOG(LOG_LEVEL_INFO, "[photo] Starting task");

            while (1)
            {
                PossibleCommand command;

                LOG(LOG_LEVEL_DEBUG, "[photo] Waiting for command");

                if (OS_RESULT_FAILED(This->_commandQueue.Pop(command, 0s)))
                {
                    This->_flags.Set(IdleFlag);
                    This->_commandQueue.Pop(command, InfiniteTimeout);
                }

                This->_flags.Clear(IdleFlag);

                LOGF(LOG_LEVEL_INFO, "[photo] Received command %d", num(command.Selected));

                switch (command.Selected)
                {
                    case Command::EnableCamera:
                        This->Invoke(command.EnableCameraCommand);
                        break;
                    case Command::DisableCamera:
                        This->Invoke(command.DisableCameraCommand);
                        break;
                    case Command::TakePhoto:
                        This->Invoke(command.TakePhotoCommand);
                        break;
                    case Command::DownloadPhoto:
                        This->Invoke(command.DownloadPhotoCommand);
                        break;
                    case Command::SavePhoto:
                        This->Invoke(command.SavePhotoCommand);
                        break;
                    case Command::Reset:
                        This->Invoke(command.ResetCommand);
                        break;
                    case Command::Sleep:
                        This->Invoke(command.SleepCommand);
                        break;
                }
            }
        }
    }
}