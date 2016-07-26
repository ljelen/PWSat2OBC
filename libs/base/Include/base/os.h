#ifndef LIBS_BASE_OS_H
#define LIBS_BASE_OS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup osal OS Abstraction layer
 * @{
 */

/**
 * @brief Maximal allowed operation timeout.
 */
#define MAX_DELAY 0xffffffffUL

/**
 * @brief Enumerator for all possible operating system error codes.
 */
typedef enum {
    /** @brief Success */
    OSResultSuccess = 0,
    /** @brief Not enough resources to complete operation. */
    OSResultOutOfResources = 1,
    /** @brief Operation timed out. */
    OSResultTimeout = 2,
    /** @brief Requested operation is invalid. */
    OSResultInvalidOperation = 3,
} OSResult;

/** @brief Type definition for time span in ms. */
typedef uint32_t OSTaskTimeSpan;

/** @brief Type definition of handle to system task. */
typedef void* OSTaskHandle;

/** @brief Type definition of semaphore handle. */
typedef void* OSSemaphoreHandle;

/** @brief Type definition of event group handle. */
typedef void* OSEventGroupHandle;

/** @brief Type definition of event group value. */
typedef uint32_t OSEventBits;

/**
 * @brief Pointer to generic system procedure that operates on task.
 *
 * @param[in] task Task handle.
 */
typedef void (*OSTaskProcedure)(OSTaskHandle task);

/**
 * @brief Pointer to procedure that creates new task.
 *
 * @param[in] entryPoint Pointer to task procedure.
 * @param[in] taskName Name of the new task.
 * @param[in] stackSize Size of the new task's stack in words.
 * @param[in] taskParameter Pointer to caller supplied object task context.
 * @param[in] priority New task priority.
 * @param[out] taskHandle Pointer to variable that will be filled with the created task handle.
 * @return Operation status.
 */
typedef OSResult (*OSTaskCreateProc)(OSTaskProcedure entryPoint,
    const char* taskName,
    uint16_t stackSize,
    void* taskParameter,
    uint32_t priority,
    OSTaskHandle* taskHandle);

/**
 * @brief Pointer to the generic system procedure.
 */
typedef void (*OSGenericProc)(void);

/**
 * @brief Pointer to the procedure that suspends current task execution for specified time period.
 *
 * @param[in] time Time period in ms.
 */
typedef void (*OSTaskSleepProc)(const OSTaskTimeSpan time);

/**
 * @brief Pointer to procedure that creates semaphore object.
 *
 * @return Semaphore handle on success, NULL otherwise.
 */
typedef OSSemaphoreHandle (*OSCreateSemaphore)(void);

/**
 * @brief Pointer to procedure that acquires requested semaphore.
 *
 * @param[in] semaphore Handle to the semaphore that should be acquired.
 * @param[in] timeout Operation timeout.
 * @return Operation status.
 */
typedef OSResult (*OSTakeSemaphore)(OSSemaphoreHandle semaphore, OSTaskTimeSpan timeout);

/**
 * @brief Pointer to procedure that releases requested semaphore.
 *
 * @param[in] semaphore Handle to semaphore that should be released.
 * @return Operation status.
 */
typedef OSResult (*OSGiveSemaphore)(OSSemaphoreHandle semaphore);

/**
 * @brief Pointer to procedure that creates new event group object.
 *
 * @return Event group handle on success, NULL otherwise.
 */
typedef OSEventGroupHandle (*OSCreateEventGroup)(void);

/**
 * @brief Pointer to procedure that modifies the event group.
 *
 * @param[in] eventGroup Handle to the event group that should be updated.
 * @param[in] bitsToChange Bits that should be set/cleared.
 */
typedef OSEventBits (*OSEventGroupChangeBits)(OSEventGroupHandle eventGroup, const OSEventBits bitsToChange);

/**
 * @brief Type of the procedure that waits for the specific bits from the event group to be set.
 *
 * @param[in] eventGroup The affected event group handle.
 * @param[in] bitsToWaitFor Bits that the caller is interested in.
 * @param[in] waitAll Flat indicating whether this procedure should return when all requested bits are set.
 * @param[in] autoReset Flag indicating whether the signaled bits should be cleared on function return.
 * @param[in] timeout Operation timeout in ms.
 *
 * @return The value of the event group at the time either the event bits being waited for became set,
 * or the block time expired.
 */
typedef OSEventBits (*OSEventGroupWaitForBits)(OSEventGroupHandle eventGroup,
    const OSEventBits bitsToWaitFor,
    bool waitAll,
    bool autoReset,
    const OSTaskTimeSpan timeout);

/**
 * @brief Type of procedure that allocates block of memory from OS heap
 * @param[in] size Size of the block to alloc
 */
typedef void* (*OSAlloc)(size_t size);

/**
 * @brief Type of procedure that frees block of memory
 * @param[in] ptr Pointer to block to free
 */
typedef void (*OSFree)(void* ptr);

/**
 * @brief Definition of operating system interface.
 */
typedef struct
{
    /**
     * @brief Pointer to function that creates new task.
     *
     * @see OSTaskCreateProc for the details.
     */
    OSTaskCreateProc CreateTask;

    /**
     * @brief Pointer to function that suspends task execution for specified time period.
     *
     * @see OSTaskSleepProc for the details.
     */
    OSTaskSleepProc SleepTask;

    /**
     * @brief Pointer to function that suspends task execution indefinitely.
     *
     * @see OSTaskProcedure for the details.
     * @remark It the task handle is NULL then this function will suspend the calling task.
     */
    OSTaskProcedure SuspendTask;

    /**
     * @brief Pointer to function that resumes execution os requested task.
     *
     * @see OSTaskProcedure for the details.
     * @remark This procedure should not be used from within interrupt service routine.
     */
    OSTaskProcedure ResumeTask;

    /**
     * @brief Pointer to procedure that runs the system scheduler.
     */
    OSGenericProc RunScheduler;

    /**
     * @brief Pointer to procedure that creates binary semaphore.
     *
     * @see OSCreateSemaphore for the details.
     */
    OSCreateSemaphore CreateBinarySemaphore;

    /**
     * @brief Pointer to procedure that acquires semaphore.
     *
     * @see OSTakeSemaphore for the details.
     * @remark This procedure should not be used from within interrupt service routine.
     */
    OSTakeSemaphore TakeSemaphore;

    /**
     * @brief Pointer to procedure that releases semaphore.
     *
     * @see OSGiveSemaphore for the details.
     * @remark This procedure should not be used from within interrupt service routine.
     */
    OSGiveSemaphore GiveSemaphore;

    /**
     * @brief Pointer to procedure that creates event group object.
     *
     * @see OSCreateEventGroup for the details.
     */
    OSCreateEventGroup CreateEventGroup;

    /**
     * @brief Pointer to procedure that sets specific bits in the event group.
     *
     * @see OSEventGroupChangeBits for the details.
     * @returns The value of the event group at the time the call to xEventGroupSetBits() returns.
     * There are two reasons why the returned value might have the bits specified by the uxBitsToSet
     * parameter cleared:
     *  - If setting a bit results in a task that was waiting for the bit leaving the blocked state
     *  then it is possible the bit will have been cleared automatically.
     *  - Any unblocked (or otherwise Ready state) task that has a priority above that of the task
     *  that called EventGroupSetBits() will execute and may change the event group value before
     *  the call to EventGroupSetBits() returns.
     * @remark This procedure should not be used from within interrupt service routine.
     */
    OSEventGroupChangeBits EventGroupSetBits;

    /**
     * @brief Pointer to procedure that clears specific bits in the event group.
     *
     * @see OSEventGroupChangeBits for the details.
     * @return The value of the event group before the specified bits were cleared.
     * @remark This procedure should not be used from within interrupt service routine.
     */
    OSEventGroupChangeBits EventGroupClearBits;

    /**
     * @brief Pointer to procedure that suspends current task execution until the
     * specific bits in the event group are set.
     *
     * @see OSEventGroupWaitForBits for the details.
     */
    OSEventGroupWaitForBits EventGroupWaitForBits;

    /**
     * @brief Pointer to procedure that allocates block of memory from OS heap
     *
     * @see OSAlloc
     */
    OSAlloc Alloc;

    /**
     * @brief Pointer to procedure that frees block of memory
     *
     * @see OSFree
     */
    OSFree Free;
} OS;

/**
 * @brief Initializes the system abstraction layer.
 */
OSResult OSSetup(void);

/** @brief System interface. */
extern OS System;

/** @}*/
#ifdef __cplusplus
}
#endif

#endif