
/**************************************************************************************************
logger.c
"logger" module for an ATSAMD51
Using SERCOM5 with ring buffers
Store snprintf()/vsnprintf created log messages into a 4K FIFO that feeds into the USART.
A single 128 byte stack buffer, "buf_compose", is used to compose a message
If the FIFO can't hold the entire message, drop the message and increment "dropped_messages" counter.
Return number of characters written to FIFO

Additional Features: When message is displayed in UART console:
Log message begins with millisecond timestamp using xTaskGetTickCount()
All log messages get an automatic "Line Feed"

**************************************************************************************************/

#include "logger.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

// SAMD / SAME definitions
#include "definitions.h"                // SYS function prototypes
#include "sam.h"

#include "logger.h"
//#include "command_line/command_line.h"

#define PRINTF_BUF_SIZE             128

volatile uint32_t dropped_messages = 0;

// print to a buffer, write buffer to SERCOM5
int log_msg(const char *fmt, ...) {
    char print_buf[PRINTF_BUF_SIZE];
    // Format user message
    va_list args;
    va_start(args, fmt);
    int msg_len = vsnprintf(print_buf, PRINTF_BUF_SIZE, fmt, args);
    va_end(args);
    // If message doesn't fit, throw it out
    //if (msg_len < 0 || msg_len > PRINTF_BUF_SIZE) return 0;

    if (msg_len < 0) return 0; // Error creating message
    // If message is too long, but we have it in the buffer, use it
    if (msg_len >= PRINTF_BUF_SIZE) msg_len = PRINTF_BUF_SIZE-1; // always a null on the end
    
    // How much space is available in TX ring buffer?
    uint32_t tx_space_available = SERCOM5_USART_WriteFreeBufferCountGet();
    if(tx_space_available >= (uint32_t)msg_len) {
        // Enough space in ring buffer...
        SERCOM5_USART_Write((uint8_t *)print_buf, msg_len);
        return msg_len;
    }
    dropped_messages++;
    return 0;
}

int cl_logger_test(void) {
	// Measure time to push out some long log messages
    uint32_t start_us = TC0_Timer32bitCounterGet(); // read us hardware timer - 937,500Hz
	// Push a minimum of 100 characters through the logger (per log message)
	// Time to push 100 characters through a USART at 115200: 100 characters * 10 bits / character * 8.68us = 8.68ms
	//
	//                   1         2         3         4         5         6         7         8         9
	log_msg("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\n");	// 1
	log_msg("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\n");
	log_msg("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\n");
	log_msg("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\n");
	log_msg("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\n");	// 5
	log_msg("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\n");
	log_msg("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\n");
	log_msg("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\n");
	log_msg("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\n");
	log_msg("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\n");	// 10
    
    uint32_t stop_us = TC0_Timer32bitCounterGet(); // read us hardware timer - 937,500Hz
    log_msg("\n10 - 100+ character messages queued in %u us",((stop_us - start_us)*16)/15);
    
	return 0;
}

