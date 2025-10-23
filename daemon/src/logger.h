/*
* PEEK - Network Monitor
*/

#ifndef PEEK_LOGGER_H
#define PEEK_LOGGER_H

#include <stdio.h>
#include <time.h>
#include <windows.h>

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_SUCCESS,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

void logger_init(void);

void logger_log(LogLevel level, const char* format, ...);

/* Convenience functions - MSVC compatible */
#define logger_debug(fmt, ...)   logger_log(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define logger_info(fmt, ...)    logger_log(LOG_INFO, fmt, ##__VA_ARGS__)
#define logger_success(fmt, ...) logger_log(LOG_SUCCESS, fmt, ##__VA_ARGS__)
#define logger_warning(fmt, ...) logger_log(LOG_WARNING, fmt, ##__VA_ARGS__)
#define logger_error(fmt, ...)   logger_log(LOG_ERROR, fmt, ##__VA_ARGS__)

#define LOG_DEBUG(...)   logger_log(LOG_DEBUG, ##__VA_ARGS__)
#define LOG_INFO(...)    logger_log(LOG_INFO, ##__VA_ARGS__)
#define LOG_SUCCESS(...) logger_log(LOG_SUCCESS, ##__VA_ARGS__)
#define LOG_WARNING(...) logger_log(LOG_WARNING, ##__VA_ARGS__)
#define LOG_ERROR(...)   logger_log(LOG_ERROR, ##__VA_ARGS__)

#endif