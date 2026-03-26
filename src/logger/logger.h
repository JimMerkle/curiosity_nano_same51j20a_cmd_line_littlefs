// logger.h

#ifndef LOGGER_H
#define LOGGER_H
#include "stdint.h"

typedef enum {
   LOGGER_ERROR,
   LOGGER_WARNING,
   LOGGER_INFO,
   LOGGER_VERBOSE,
   LOGGER_CONSOLE,
   LOGGER_CYAN,
   LOGGER_MAGENTA,
   LOGGER_BLUE
} LOGGER_PRIORITY;

// Single interface to manage multiple logging requirements
int logger_message(LOGGER_PRIORITY priority, const char *fmt, ...);

// The following macros use the above logging interface to produce different colors/format based on 'priority'
#define log_error(fmt, ...)   logger_message(LOGGER_ERROR, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...)    logger_message(LOGGER_WARNING, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...)    logger_message(LOGGER_INFO, fmt, ##__VA_ARGS__)
#define log_verbose(fmt, ...) logger_message(LOGGER_VERBOSE, fmt, ##__VA_ARGS__)
#define log_msg(fmt, ...)     logger_message(LOGGER_CONSOLE, fmt, ##__VA_ARGS__)   /* console - white text, no time-stamp, no auto LF */
#define log_cyan(fmt, ...)    logger_message(LOGGER_CYAN, fmt, ##__VA_ARGS__)
#define log_magenta(fmt, ...) logger_message(LOGGER_MAGENTA, fmt, ##__VA_ARGS__)
#define log_blue(fmt, ...)    logger_message(LOGGER_BLUE, fmt, ##__VA_ARGS__)

extern volatile uint32_t dropped_messages;

#endif // LOGGER_H

