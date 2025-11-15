// logger.h

#ifndef LOGGER_H
#define LOGGER_H
#include "stdint.h"

int log_msg(const char *fmt, ...);
extern volatile uint32_t dropped_messages;

#endif // LOGGER_H

