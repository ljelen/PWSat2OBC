#include "os.hpp"
#include <utility>

static IOS* OSProxy = nullptr;

OSResult System::CreateTask(OSTaskProcedure entryPoint, //
    const char* taskName,
    uint16_t stackSize,
    void* taskParameter,
    uint32_t priority,
    OSTaskHandle* taskHandle)
{
    if (OSProxy != nullptr)
    {
        return OSProxy->CreateTask(entryPoint, taskName, stackSize, taskParameter, priority, taskHandle);
    }
    else
    {
        return OSResultNotSupported;
    }
}

void System::RunScheduler(void)
{
    if (OSProxy != nullptr)
    {
        OSProxy->RunScheduller();
    }
}

void System::SleepTask(const OSTaskTimeSpan time)
{
    if (OSProxy != nullptr)
    {
        OSProxy->Sleep(time);
    }
}

void System::SuspendTask(OSTaskHandle task)
{
    if (OSProxy != nullptr)
    {
        OSProxy->SuspendTask(task);
    }
}

void System::ResumeTask(OSTaskHandle task)
{
    if (OSProxy != nullptr)
    {
        OSProxy->ResumeTask(task);
    };
}

OSSemaphoreHandle System::CreateBinarySemaphore()
{
    if (OSProxy != nullptr)
    {
        return OSProxy->CreateBinarySemaphore();
    }

    return nullptr;
}

OSResult System::GiveSemaphore(OSSemaphoreHandle semaphore)
{
    if (OSProxy != nullptr)
    {
        return OSProxy->GiveSemaphore(semaphore);
    }

    return OSResultInvalidOperation;
}

OSResult System::TakeSemaphore(OSSemaphoreHandle semaphore, OSTaskTimeSpan timeout)
{
    if (OSProxy != nullptr)
    {
        return OSProxy->TakeSemaphore(semaphore, timeout);
    }

    return OSResultInvalidOperation;
}

OSEventGroupHandle System::CreateEventGroup(void)
{
    if (OSProxy != nullptr)
    {
        return OSProxy->CreateEventGroup();
    }

    return nullptr;
}

OSEventBits System::EventGroupSetBits(OSEventGroupHandle eventGroup, const OSEventBits bitsToChange)
{
    if (OSProxy != nullptr)
    {
        return OSProxy->EventGroupSetBits(eventGroup, bitsToChange);
    }

    return 0;
}

OSEventBits System::EventGroupClearBits(OSEventGroupHandle eventGroup, const OSEventBits bitsToChange)
{
    if (OSProxy != nullptr)
    {
        return OSProxy->EventGroupClearBits(eventGroup, bitsToChange);
    }

    return 0;
}

OSEventBits System::EventGroupWaitForBits(
    OSEventGroupHandle eventGroup, const OSEventBits bitsToWaitFor, bool waitAll, bool autoReset, const OSTaskTimeSpan timeout)
{
    if (OSProxy != nullptr)
    {
        return OSProxy->EventGroupWaitForBits(eventGroup, bitsToWaitFor, waitAll, autoReset, timeout);
    }

    return 0;
}

void* System::Alloc(size_t size)
{
    if (OSProxy != nullptr)
    {
        return OSProxy->Alloc(size);
    }

    return nullptr;
}

void System::Free(void* ptr)
{
    if (OSProxy != nullptr)
    {
        OSProxy->Free(ptr);
    }
}

OSQueueHandle System::CreateQueue(size_t maxElementCount, size_t elementSize)
{
    if (OSProxy != nullptr)
    {
        return OSProxy->CreateQueue(maxElementCount, elementSize);
    }

    return nullptr;
}

bool System::QueueReceive(OSQueueHandle queue, void* element, OSTaskTimeSpan timeout)
{
    if (OSProxy != nullptr)
    {
        return OSProxy->QueueReceive(queue, element, timeout);
    }

    return false;
}

bool System::QueueReceiveFromISR(OSQueueHandle queue, void* element)
{
    if (OSProxy != nullptr)
    {
        return OSProxy->QueueReceiveFromISR(queue, element);
    }

    return false;
}

bool System::QueueSend(OSQueueHandle queue, void* element, OSTaskTimeSpan timeout)
{
    if (OSProxy != nullptr)
    {
        return OSProxy->QueueSend(queue, element, timeout);
    }

    return false;
}

bool System::QueueSendISR(OSQueueHandle queue, void* element)
{
    if (OSProxy != nullptr)
    {
        return OSProxy->QueueSendISR(queue, element);
    }

    return false;
}

void System::QueueOverwrite(OSQueueHandle queue, const void* element)
{
    if (OSProxy != nullptr)
    {
        OSProxy->QueueOverwrite(queue, element);
    }
}

OSPulseHandle System::CreatePulseAll(void)
{
    if (OSProxy != nullptr)
    {
        return OSProxy->CreatePulseAll();
    }

    return 0;
}

OSResult System::PulseWait(OSPulseHandle handle, OSTaskTimeSpan timeout)
{
    if (OSProxy != nullptr)
    {
        return OSProxy->PulseWait(handle, timeout);
    }

    return OSResultNotSupported;
}

void System::PulseSet(OSPulseHandle handle)
{
    if (OSProxy != nullptr)
    {
        OSProxy->PulseSet(handle);
    }
}

void System::EndSwitchingISR()
{
    if (OSProxy != nullptr)
    {
        OSProxy->EndSwitchingISR();
    }
}

OSReset::OSReset() : released(false)
{
}

OSReset::OSReset(OSReset&& arg) noexcept : released(arg.released)
{
    arg.released = true;
}

OSReset& OSReset::operator=(OSReset&& arg) noexcept
{
    OSReset tmp(std::move(arg));
    this->released = tmp.released;
    tmp.released = true;
    return *this;
}

OSReset::~OSReset()
{
    if (!released)
    {
        OSProxy = nullptr;
    }
}

OSReset InstallProxy(IOS* target)
{
    OSProxy = target;

    return OSReset();
}
