#include <assert.h> // static_assert
#include <stdint.h>
#include <stdio.h>
#include <string.h> //memset
#include "logger.h"
#include "system.h"

/**
 * @addtogroup Logger
 * @{
*/

/** @brief Logger endpoint number limit. */
#define MAX_ENDPOINTS 3

static_assert(MAX_ENDPOINTS < UINT8_MAX, "Fix type of logger's endpoint counter: 'Logger::endpointCount'. ");

/**
 * @brief This type describes single logger endpoint.
 */
typedef struct
{
    /**
     * @brief Endpoint execution context.
     */
    void* context;

    /**
     * @brief Endpoint entry point.
     */
    LoggerProcedure endpoint;

    /**
     * @brief Custom endpoint logging level.
     */
    enum LogLevel endpointLogLevel;
} LoggerEndpoint;

/** @brief This type describes the logger object. */
typedef struct
{
    /** @brief Number of currently available logger endpoints. */
    uint8_t endpointCount;

    /** @brief Global logger loggin level. */
    enum LogLevel globalLevel;

    /** @brief Array for logger endpoints. */
    LoggerEndpoint endpoints[MAX_ENDPOINTS];
} Logger;

/** @brief Global logger object. */
static Logger logger = {};

/** @brief Array for converting log level to string. */
static const char* const levelMap[] = {"[Always]  ", "[Fatal]   ", "[Error]   ", "[Warning] ", "[Info]    ", "[Debug]   ", "[Trace]   "};

static_assert(LOG_LEVEL_ALWAYS == 0, "Fix level conversion map for level: Always");
static_assert(LOG_LEVEL_FATAL == 1, "Fix level conversion map for level: Fatal");
static_assert(LOG_LEVEL_ERROR == 2, "Fix level conversion map for level: Error");
static_assert(LOG_LEVEL_WARNING == 3, "Fix level conversion map for level: Warning");
static_assert(LOG_LEVEL_INFO == 4, "Fix level conversion map for level: Info");
static_assert(LOG_LEVEL_DEBUG == 5, "Fix level conversion map for level: Debug");
static_assert(LOG_LEVEL_TRACE == 6, "Fix level conversion map for level: Trace");

/**
 * @brief Convert log level to string.
 * @param[in] level Logging level to convert.
 * @return String representation of logging level.
 * @remark Logging level strings are already aligned. If passed log level is not known then the default string will be returned.
 */
static const char* LogConvertLevelToString(enum LogLevel level)
{
    if (level >= COUNT_OF(levelMap))
    {
        return "[Unknown] ";
    }
    else
    {
        return levelMap[level];
    }
}

void LogInit(enum LogLevel globalLogLevel)
{
    logger.globalLevel = globalLogLevel;
    logger.endpointCount = 0;
    memset(logger.endpoints, 0, sizeof(logger.endpoints));
}

bool LogAddEndpoint(LoggerProcedure endpoint, void* context, enum LogLevel endpointLogLevel)
{
    if (logger.endpointCount >= MAX_ENDPOINTS)
    {
        return false;
    }

    LoggerEndpoint endpointDescription;
    endpointDescription.endpoint = endpoint;
    endpointDescription.context = context;
    endpointDescription.endpointLogLevel = endpointLogLevel;

    logger.endpoints[logger.endpointCount++] = endpointDescription;
    return true;
}

void LogRemoveEndpoint(LoggerProcedure endpoint)
{
    for (uint8_t cx = 0; cx < logger.endpointCount; ++cx)
    {
        if (logger.endpoints[cx].endpoint == endpoint)
        {
            memmove(logger.endpoints + cx, logger.endpoints + cx + 1, sizeof(*logger.endpoints) * (logger.endpointCount - (cx + 1)));
            --logger.endpointCount;
            break;
        }
    }
}

/**
 * @brief This procedure determines if passed logging level is considered to be enabled on configured logging level.
 * @param[in] requestedLogLEvel Queried log level.
 * @param[in] currentLogLevel Currently used logging level
 * @return True Then passed logging level is enabled, false otherwise.
 */
static inline bool CanLogAtLevel(const enum LogLevel requestedLogLEvel, const enum LogLevel currentLogLevel)
{
    return requestedLogLEvel <= currentLogLevel;
}

void LogMessage(bool withinIsr, enum LogLevel messageLevel, const char* message, ...)
{
    if (!CanLogAtLevel(messageLevel, logger.globalLevel))
    {
        return;
    }

    const char* header = LogConvertLevelToString(messageLevel);

    va_list arguments;
    va_start(arguments, message);

    for (uint8_t cx = 0; cx < logger.endpointCount; ++cx)
    {
        const LoggerEndpoint* endpoint = &logger.endpoints[cx];
        if (CanLogAtLevel(messageLevel, endpoint->endpointLogLevel))
        {
            endpoint->endpoint(endpoint->context, withinIsr, header, message, arguments);
        }
    }

    va_end(arguments);
}

/** @} */
