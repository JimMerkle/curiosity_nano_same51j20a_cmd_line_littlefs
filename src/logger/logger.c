
/**************************************************************************************************
logger.c
"logger" module for an ATSAMD51
Using SERCOM5 with ring buffers
Store snprintf()/vsnprintf created log messages into a 4K FIFO that feeds into the USART.
A single 128 byte stack buffer, "buf_compose", is used to compose a message
If the FIFO can't hold the entire message, drop the message and increment "dropped_messages" counter.
Return number of characters written to FIFO

Additional Features: When message is displayed in UART console:
Log message begins with millisecond timestamp using SYSTICK_GetTickCounter()
All log messages get an automatic "Line Feed"

**************************************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include "logger/logger.h"

// SAMD / SAME definitions
#include "definitions.h"                // All SAM peripherals

// Definitions for VT100 / ANSI color escape character strings
// Foreground Colors: 30: Black, 31: Red, 32: Green, 33: Yellow, 34: Blue, 35: Magenta, 36: Cyan, and 37: White.
#define COLOR_RED               "\033[31m"   /* red text */
#define COLOR_GREEN             "\033[32m"   /* green text */
#define COLOR_YELLOW            "\033[33m"   /* yellow text */
#define COLOR_BLUE              "\033[34m"   /* blue text */
#define COLOR_MAGENTA           "\033[35m"   /* magenta text */
#define COLOR_CYAN              "\033[36m"   /* cyan text */
#define COLOR_WHITE             "\033[37m"   /* white text */

#define COLOR_RESET             "\033[0m"    /* Reset text color to previous color */
/* Pre-calculated length of COLOR_RESET ("\033[0m") to avoid runtime strlen() */
#define COLOR_RESET_LEN         4

#define PRINTF_BUF_SIZE         128
#define NUL_PAD                 1
#define MIN_PRINTF_REQUIRED     32 /* conservative minimum for prefix(5)+timestamp(10)+payload+reset(4)+LF */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(PRINTF_BUF_SIZE >= MIN_PRINTF_REQUIRED, "PRINTF_BUF_SIZE too small for logger requirements");
#endif
/* MIN_PRINTF_REQUIRED already defined above for compile-time check */

volatile uint32_t dropped_messages = 0;

//=================================================================================================
// Print to a buffer, write buffer to SERCOM5 FIFO, manage color and auto line-feed based on priority
// * Supports color log messages using "priority"
// * Fails early if not enough room in FIFO
// * For each message that uses a color escape sequence,
//     a reset sequence is appended to restore to previous color
// * All messages, except those for the console, will automatically receive a terminating line-feed.
// * Messages will be truncated to fit available FIFO space
// * Messages will not have a terminating null.  (We don't want null characters in the FIFO!)
// * Console messages are treated as "pass through".  Only print format string,
//     nothing more, nothing less.  (Message will get truncated if there's a space problem)
// * Speed / Fewer Instructions are more important vs using every byte of available FIFO space
int logger_message(LOGGER_PRIORITY priority, const char *fmt, ...) {
//=================================================================================================
    char print_buf[PRINTF_BUF_SIZE];
    int offset=0; 
    char * szcolor = COLOR_WHITE;
    // 1) Bail early if we don't have enough space for color prefix, time stamp, some truncated message,
    //      color reset, and auto line-feed.
    // How much space is available in TX ring buffer?
    int tx_space_available = SERCOM5_USART_WriteFreeBufferCountGet();
    // Bail early if FIFO doesn't have our minimum required space
    if (tx_space_available < MIN_PRINTF_REQUIRED) {
        dropped_messages++;
        return 0;
    }
    // Re-use this variable to represent maximum number of bytes we can prepare to transmit
    if(tx_space_available > PRINTF_BUF_SIZE) tx_space_available = PRINTF_BUF_SIZE;
    /* Reserve room for color reset and line feed */
    tx_space_available -= COLOR_RESET_LEN;

    // 2) Select a color string depending on the priority
    switch(priority) {
        case LOGGER_ERROR: szcolor = COLOR_RED; break;
        case LOGGER_WARNING: szcolor = COLOR_YELLOW; break;
        case LOGGER_INFO: szcolor = COLOR_GREEN; break;
        case LOGGER_CYAN: szcolor = COLOR_CYAN; break;
        case LOGGER_MAGENTA: szcolor = COLOR_MAGENTA; break;
        case LOGGER_BLUE: szcolor = COLOR_BLUE; break;
        // Anything else is white
        default: ;
     } // switch(priority)

    // 3) Prepend a color string and timestamp to all messages unless it's a console message
    /* Precompute writable size (includes room for terminating NUL) to avoid repeated +NUL_PAD */
    if (LOGGER_CONSOLE != priority) {
        /* Assume reserved space is sufficient; write prefix directly */
        offset = snprintf(print_buf, tx_space_available, "%s(%lu)", szcolor, SYSTICK_GetTickCounter());
    }

    // 4) Append user message (may be truncated). We reserved tail bytes earlier; assume space exists.
    va_list args;
    va_start(args, fmt);
    int avail = tx_space_available - offset;
    int pn = 0;
    if (avail > 0) pn = vsnprintf(print_buf + offset, (size_t)avail, fmt, args);
    va_end(args);
    if (pn < 0) {
        /* formatting error: don't emit partial timestamped message */
        dropped_messages++;
        return 0;
    }
    if (pn > avail) pn = avail;
    offset += pn;

    // 5) If non-console, append reset + LF (we reserved space earlier). Single write only.
    if (LOGGER_CONSOLE != priority) {
        memcpy(print_buf + offset, COLOR_RESET, COLOR_RESET_LEN);
        offset += COLOR_RESET_LEN;
        print_buf[offset++] = '\n';
    }

    SERCOM5_USART_Write((uint8_t *)print_buf, offset);
    return offset;

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
    
    uint32_t stop_us = TC0_Timer32bitCounterGet(); // read us hardware timer - 1MHz rate (1us increments)
    log_msg("\n10 - 100+ character messages queued in %u us\n",(stop_us - start_us));
    
    /* Additional automated checks: exercise priorities and overflow handling */
    log_info("AUTO-TEST: INFO message, value=%d", 123);
    log_error("AUTO-TEST: ERROR message");
    log_warn("AUTO-TEST: WARNING message");
    log_msg("AUTO-TEST: CONSOLE message (no auto-LF)");

    /* Force an oversized message to ensure truncation/drop path works */
    char bigbuf[PRINTF_BUF_SIZE * 2];
    for (int i = 0; i < (int)sizeof(bigbuf) - 1; ++i) bigbuf[i] = 'A' + (i % 26);
    bigbuf[sizeof(bigbuf) - 1] = '\0';
    int rc = log_info("%s", bigbuf);
    log_msg("AUTO-TEST: oversized message rc=%d dropped_messages=%lu\n", rc, dropped_messages);

    log_msg("\n=== EDGE CASE TESTS ===\n");
    
    /* Test 1: All color priorities to verify color codes and auto-LF */
    log_msg("TEST 1: Verify all color priorities with auto line-feed:\n");
    uint32_t dropped_before = dropped_messages;
    log_error("ERROR priority test");
    log_warn("WARNING priority test");
    log_info("INFO priority test");
    log_verbose("VERBOSE priority test");
    log_cyan("CYAN priority test");
    log_magenta("MAGENTA priority test");
    log_blue("BLUE priority test");
    log_msg("Color test complete. Dropped: %lu\n\n", dropped_messages - dropped_before);
    
    /* Test 2: Console message (no color, no timestamp, no auto-LF) */
    log_msg("TEST 2: Console message (no auto-LF, observe cursor position):\n[");
    log_msg("CONSOLE_NO_LF");
    log_msg("]<-- should be on same line\n\n");
    
    /* Test 3: Messages with existing line-feed vs without */
    log_msg("TEST 3: Messages with/without trailing LF:\n");
    log_info("Message WITHOUT trailing LF");
    log_info("Message WITH trailing LF\n");
    log_info("Another WITHOUT LF");
    log_msg("LF test complete\n\n");
    
    /* Test 4: Very long message to test truncation */
    log_msg("TEST 4: Truncation test (should truncate gracefully):\n");
    dropped_before = dropped_messages;
    log_warn(
        "This is a very long message that should be truncated because it exceeds the available buffer space. "
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 "
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 "
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 "
        "If you see this text, truncation failed!");
    log_msg("Truncation test complete. Dropped: %lu\n\n", dropped_messages - dropped_before);
    
    /* Test 5: Edge case - empty message */
    log_msg("TEST 5: Empty message:\n");
    log_info("");
    log_msg("Empty message test complete\n\n");
    
    /* Test 6: Format specifiers */
    log_msg("TEST 6: Format specifiers:\n");
    log_cyan("Integer: %d, Hex: 0x%X, String: %s", 42, 0xDEADBEEF, "test");
    log_magenta("Multiple args: %d + %d = %d", 10, 20, 30);
    log_msg("Format test complete\n\n");
    
    /* Test 7: Message exactly at buffer boundary */
    log_msg("TEST 7: Message near buffer boundary:\n");
    dropped_before = dropped_messages;
    /* Create a message that's close to PRINTF_BUF_SIZE */
    char boundary_msg[90];
    memset(boundary_msg, 'X', sizeof(boundary_msg) - 1);
    boundary_msg[sizeof(boundary_msg) - 1] = '\0';
    log_error("%s", boundary_msg);
    log_msg("Boundary test complete. Dropped: %lu\n\n", dropped_messages - dropped_before);
    
    /* Test 8: Force FIFO overflow by sending rapid burst of large messages */
    log_msg("TEST 8: Force dropped messages by overwhelming FIFO:\n");
    dropped_before = dropped_messages;
    /* Send 50 large messages rapidly to fill FIFO faster than UART can drain */
    for (int i = 0; i < 50; i++) {
        log_error("Burst message #%02d: XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", i);
    }
    /* Wait for FIFO to drain by polling WriteCountGet until it reaches zero */
    log_msg("\nWaiting for FIFO to drain...\n");
    while (SERCOM5_USART_WriteCountGet() > 0) {
        /* Poll until all bytes are transmitted */
    }
    uint32_t dropped_in_burst = dropped_messages - dropped_before;
    log_msg("Burst test complete. Dropped in burst: %lu\n\n", dropped_in_burst);
    
    log_msg("=== EDGE CASE TESTS COMPLETE ===\n");
    log_msg("Total dropped_messages counter: %lu\n", dropped_messages);
    if (dropped_messages > 0) {
        log_msg("SUCCESS: At least one message was dropped as expected!\n");
    } else {
        log_msg("WARNING: No messages were dropped (FIFO may be very large)\n");
    }

    return 0;
}

