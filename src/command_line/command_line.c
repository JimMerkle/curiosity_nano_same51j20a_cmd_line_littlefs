// Copyright Jim Merkle, 2/17/2020
// Module: command_line.c
//
// Command Line Parser
//
// Using serial interface, receive commands with parameters.
// Parse the command and parameters, look up the command in a table, execute the command.
// Since the command/argument buffer is global with global pointers, each command will parse its own arguments.
// Since no arguments are passed in the function call, all commands will have int command_name(void) prototype.

// Notes:
// The stdio library's stdout stream is buffered by default.  This makes printf() and putchar() work strangely
// for character I/O.  Buffering needs to be disabled for this code module.  See setvbuf() in cl_setup().

//#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>                     // uint8_t
#include <stdio.h>                      // EOF
#include "command_line.h"
#include "littlefs/w25q128.h"
#include "littlefs/lfs_interface.h"

// SAMD / SAME definitions
#include "definitions.h"                // SYS function prototypes
#include "sam.h"
#include "logger/logger.h"     // send output though "logger" module
#include "version.h"
#include "ymodem/ymodem.h"
#include "ini_store_v2/ini_store_v2.h"

// Typedefs
typedef struct {
  char * command;
  char * comment;
  int (*function)(void); // pointer to command function
} COMMAND_ITEM;

const COMMAND_ITEM cmd_table[] = {
    {"?",         "display help menu",                                      cl_help},
    {"help",      "display help menu",                                      cl_help},
    {"cls",       "clear screen",                                           cl_cls},
    {"add",       "add <number> <number>",                                  cl_add},
    {"id",        "unique ID",                                              cl_id},
    {"reset",     "reset processor",                                        cl_reset},
    {"info",      "processor info",                                         cl_info},
    {"timer",     "timer test - measure 50ms OS sleep",                     cl_timer},
    {"logger",    "Log message test",                                       cl_logger_test},
    {"sx",        "send xmodem <file>",                                     cl_xmodem_send},
    {"rx",        "receive xmodem <file>",                                  cl_xmodem_receive},
    {"ymodem",    "ymodem <file> to send a file, else receive file",        cl_ymodem},
    {"version",   "display firmware version",                               cl_version},
    {"initest",   "test the ini_store key-value pair storage",              cl_ini_store_test},
    {"iniset",    "iniset <file> <key> <value>",                            cl_iniset},
    {"iniget",    "iniget <file> <key>",                                    cl_iniget},
//  {"erasetimes","dump erasing timing collected",                          cl_dump_sector_erase_times},
    {"==========","==========================================",             NULL},
    FLASH_TEST_COMMANDS,
    {"==========","==========================================",             NULL},
    LITTLEFS_COMMANDS,
    {NULL,NULL,NULL}, /* end of table */
};

// Globals:
char buffer[MAXSERIALBUF]; // holds command strings from user
char * argv[MAXWORDS]; // pointers into buffer
int argc; // number of words (command & arguments)

// Enable command line history.  Comment out to remove history buffer and code.
#define COMMAND_LINE_HISTORY 1

#ifdef COMMAND_LINE_HISTORY
#define MAX_COMMAND_HISTORY 20 /* remember last 20 commands */
// Command history support
static char history[MAX_COMMAND_HISTORY][MAXSERIALBUF];
static int history_count = 0; // number of valid entries
static int history_pos = 0;   // next slot to write
static int history_view = -1; // -1 = not viewing, else index into history[]
#endif

// Implement a getchar function, needed for Command Line
// If character available, return character, else return EOF
int __io_getchar(void) {
    uint8_t data;
    if(SERCOM5_USART_ReadCountGet()) {
        // We have data
        SERCOM5_USART_Read(&data, sizeof(data));
        return data;
    } else return EOF;
} // __io_getchar())


void cl_setup(void) {
    char banner[200];
    // Turn on yellow on blue text, print greeting, version, date, reset color
    snprintf(banner,sizeof(banner),"Command Line - LittleFS, %s, %s %s\n"
                                   "Enter \"help\" or \"?\" for list of commands",
                                   PROJECT_VERSION, __DATE__, __TIME__);
    text_in_box(banner,COLOR_YELLOW_ON_BLUE);
    log_msg(">"); // initial prompt
}

// Check for data available from USART interface.  If none present, just return.
// If data available, process it (add it to character buffer if appropriate)
void cl_loop(void)
{
  static int index = 0; // index into global buffer
  int c;
  // Command line history editing state
#ifdef COMMAND_LINE_HISTORY
  static int cursor = 0; // position of cursor in buffer
  static int editing = 0; // 0 = normal typing, 1 = editing mode
#endif

  // Spin, reading characters from FIFO until EOF character is received (no data), buffer is full, or
  // a <line feed> character is received.  Null terminate the global string, don't return the <LF>
  while (1) {
    c = __io_getchar();
    switch (c) {
      case EOF:
        return; // non-blocking - return
#ifdef COMMAND_LINE_HISTORY
      // VT100/ANSI escape sequence handling for arrow keys
      case 0x1B: { // ESC
        int next1 = __io_getchar();
        if (next1 == '[') {
          int next2 = __io_getchar();
          switch (next2) {
            case 'A': // Up arrow
              if (history_count > 0) {
                if (history_view < 0)
                  history_view = history_pos - 1;
                else if (history_view > 0)
                  history_view--;
                else
                  history_view = 0;
                if (history_view < 0)
                  history_view = history_count - 1;
                // Copy history to buffer
                strncpy(buffer, history[history_view], MAXSERIALBUF);
                buffer[MAXSERIALBUF - 1] = 0;
                index = strlen(buffer);
                cursor = index;
                // Redraw line
                log_msg("\r>%-*s", MAXSERIALBUF - 1, buffer);
                // Move cursor to end
                log_msg("\r>%s", buffer);
              }
              continue;
            case 'B': // Down arrow
              if (history_count > 0 && history_view >= 0) {
                history_view++;
                if (history_view >= history_count) {
                  // Clear buffer if past end
                  buffer[0] = 0;
                  index = 0;
                  cursor = 0;
                  history_view = -1;
                } else {
                  strncpy(buffer, history[history_view], MAXSERIALBUF);
                  buffer[MAXSERIALBUF - 1] = 0;
                  index = strlen(buffer);
                  cursor = index;
                }
                // Redraw line
                log_msg("\r>%-*s", MAXSERIALBUF - 1, buffer);
                log_msg("\r>%s", buffer);
              }
              continue;
            case 'C': // Right arrow
              if (cursor < index) {
                cursor++;
                log_msg("\x1B[C");
              }
              editing = 1;
              continue;
            case 'D': // Left arrow
              if (cursor > 0) {
                cursor--;
                log_msg("\x1B[D");
              }
              editing = 1;
              continue;
          }
        }
        continue;
      }
#endif // COMMAND_LINE_HISTORY
      case _CR:
      case _LF:
        buffer[index] = 0; // null terminate
        if (index) {
          log_msg("\n"); // newline
#ifdef COMMAND_LINE_HISTORY
          // Save to history if not duplicate of last
          if (index > 0 && (history_count == 0 || strcmp(buffer, history[(history_pos - 1 + MAX_COMMAND_HISTORY) % MAX_COMMAND_HISTORY]) != 0)) {
            strncpy(history[history_pos], buffer, MAXSERIALBUF);
            history[history_pos][MAXSERIALBUF - 1] = 0;
            history_pos = (history_pos + 1) % MAX_COMMAND_HISTORY;
            if (history_count < MAX_COMMAND_HISTORY)
              history_count++;
          }
          history_view = -1;
#endif
          cl_process_buffer(); // process the null terminated buffer
#ifdef COMMAND_LINE_HISTORY
          cursor = 0;
          editing = 0;
#endif
        }
        log_msg("\n>");
        index = 0;
        return;
      case _BS:
      case 0x7F: // DEL
        if (index < 1)
          continue;
#ifdef COMMAND_LINE_HISTORY
        if (cursor > 0) {
          // Remove character before cursor
          memmove(&buffer[cursor - 1], &buffer[cursor], index - cursor + 1);
          index--;
          cursor--;
          // Redraw line
          log_msg("\r>%-*s", MAXSERIALBUF - 1, buffer);
          // Move cursor to correct position
          log_msg("\r>");
          for (int i = 0; i < cursor; i++)
            log_msg("\x1B[C");
        }
        continue;
#else
        log_msg("\b \b"); // remove the previous character from the screen and buffer
        index--;
        continue;
#endif
      default:
        // If printable character, display and store it
        if (index < (MAXSERIALBUF - 1) && c >= ' ' && c <= '~') {
#ifdef COMMAND_LINE_HISTORY
          if (!editing) {
            // Normal typing: always append
            buffer[index] = (char)c;
            index++;
            cursor = index;
            log_msg("%c", c);
          } else {
            // Editing mode: insert at cursor
            memmove(&buffer[cursor + 1], &buffer[cursor], index - cursor);
            buffer[cursor] = (char)c;
            index++;
            cursor++;
            // Redraw line
            log_msg("\r>%-*s", MAXSERIALBUF - 1, buffer);
            log_msg("\r>");
            for (int i = 0; i < cursor; i++)
              log_msg("\x1B[C");
          }
#else
          log_msg("%c", c); // write character to terminal
          buffer[index] = (char)c;
          index++;
#endif
        }
    } // switch
  } // while (1)
  return;
} // cl_loop()

void cl_process_buffer(void)
{
    argc = cl_parseArgcArgv(buffer, argv, MAXWORDS);
    // Display each of the "words" / command and arguments
    //for(int i=0;i<argc;i++)
    //  log_msg("%d >%s<\n",i,argv[i]);
    if (argc) {
        // At least one "word" / argument found
        // See if command has a match in the command table
        // If null command pointer found, exit for-loop
        int cmdIndex;
        for (cmdIndex = 0; cmd_table[cmdIndex].command; cmdIndex++) {
            if (strcmp(argv[0], cmd_table[cmdIndex].command) == 0) {
                // We found a match in the table
                // Call the function associated with the command
                (*cmd_table[cmdIndex].function)();
                break; // exit for-loop
            }
        } // for-loop
          // If we compared all the command strings and didn't find the command, or we want to fake that event
        if (!cmd_table[cmdIndex].command) {
            log_msg("Command \"%s\" not found\n", argv[0]);
        }
    } // At least one "word" / argument found
}

// Return true (non-zero) if character is a white space character
int cl_isWhiteSpace(char c) {
  if(c==' ' || c=='\t' ||  c=='\r' || c=='\n' )
    return 1;
  else
    return 0;
}

// Parse string into arguments/words, returning count
// Required an array of char pointers to store location of each word, and number of strings available
// "count" is the maximum number of words / parameters allowed
int cl_parseArgcArgv(char * inBuf,char **words, int count)
{
  int wordcount = 0;
  while(*inBuf) {
    // We have at least one character
    while(cl_isWhiteSpace(*inBuf)) inBuf++; // remove leading whitespace
    if(*inBuf) {// have a non-whitespace
      if(wordcount < count) {
        // If pointing at a double quote, need to remove/advance past the first " character
        // and find the second " character that goes with it, and remove/advance past that one too.
        if(*inBuf == '\"' && inBuf[1]) {
            // Manage double quoted word
            inBuf++; // advance past first double quote
            words[wordcount]=inBuf; // point at this new word
            wordcount++;
            while(*inBuf && *inBuf != '\"') inBuf++; // move to end of word (next double quote)
        } else {
            // normal - not double quoted string
            words[wordcount]=inBuf; // point at this new word
            wordcount++;
            while(*inBuf && !cl_isWhiteSpace(*inBuf)) inBuf++; // move to end of word
        }
        if(cl_isWhiteSpace(*inBuf) || *inBuf == '\"') { // null terminate this word
          *inBuf=0;
          inBuf++;
        }
      } // if(wordcount < count)
      else {
        *inBuf=0; // null terminate string
        break; // exit while-loop
      }
    }
  } // while(*inBuf)
  return wordcount;
} // parseArgcArgv()

// We may want to add a comment/description field to the table to describe each command
int cl_help(void) {
    log_msg("Help - command list\n");
    log_msg("Command     Comment\n");
    // Walk the command array, displaying each command
    // Continue until null function pointer found
    for (int i = 0; cmd_table[i].command; i++) {
        log_msg("%-12s%s\n",cmd_table[i].command,cmd_table[i].comment);
    }
    log_msg("\n");
    return 0;
}

// Clear the screen
int cl_cls(void) {
   // VT-100 command to clear the screen and move cursor to upper left corner
   log_msg("\x1B[2J\x1B[H");
   return 0;
}

int cl_add(void) {
    // verify argument count, to remove it from the table
    if(argc < 3) {
        log_msg("\nInvalid Arg cnt: %d Expected: %d\n", argc - 1, 2);
        return 0;
    }
    log_msg("add..  A: %s  B: %s\n", argv[1], argv[2]);
    int A = (int) strtol(argv[1], NULL, 0); // allow user to use decimal or hex
    int B = (int) strtol(argv[2], NULL, 0);
    int ret = A + B;
    log_msg("returning %d\n\n", ret);
    return ret;
}

/*
Serial Number - From section 9.6 of SAM-D5x-E5x-Family-Data-Sheet-DS60001507.pdf 
Each device has a unique 128-bit serial number which is a concatenation of four 32-bit words
contained at the following addresses:
Word 0: 0x008061FC
Word 1: 0x00806010
Word 2: 0x00806014
Word 3: 0x00806018
The uniqueness of the serial number is guaranteed only when using all 128 bits.

Read and display unique ID for SAME51 - 4 - 32-bit words (128 bits) */
uint32_t unique_id[4]; // keep this as a global for now
int cl_id(void) {
    unique_id[0] = *(uint32_t *)0x008061FC;
    unique_id[1] = *(uint32_t *)0x00806010;
    unique_id[2] = *(uint32_t *)0x00806014;
    unique_id[3] = *(uint32_t *)0x00806018;
    log_msg("ID: %08X %08X %08X %08X\n",unique_id[3], unique_id[2], unique_id[1], unique_id[0]);
    return 0;
}

// Reset the processor
int cl_reset(void) {
    NVIC_SystemReset(); // CMSIS Cortex-M4 function - see Drivers/CMSIS/Include/core_cm4.h
    // The above function has its own spin loop to wait for completion
    //while (1) ; // wait here until reset completes
    return 0;
}

// Compare multiple timers..  SYSTICK Timer, TC0 Timer, 
// TC0 timer is running at 15/16 MHz.  Multiply by 16, then divide by 15 for uSeconds
// The 937,500Hz frequency works very well and extends the range of the counter 16X vs 15MHz.
// The counter will rollover after 4581 seconds (76 minutes)
// Add DWT->CYCCNT usage - This 32-bit counter rolls over every 35.79 seconds
int cl_timer(void) {
    log_msg("%s(), Timing SYSTICK_DelayMs(50)\n",__func__);
    uint32_t start_us = TC0_Timer32bitCounterGet(); // read us hardware timer
    SYSTICK_DelayMs(50);
    uint32_t stop_us = TC0_Timer32bitCounterGet(); // read us hardware timer
    // Report results
    log_msg("TC0_Timer32bit time: %lu us\n",stop_us - start_us);
    return 0;
}

//=================================================================================================
// Read the Device Service Unit (DSU) - Device IDentification register (DID)
// Expected value for Expected value for ATSAME51J20A: 0x61810304
//=================================================================================================
// My Curiosity Nano board displayed: Processor: 6, Family: 3, Series: 1, Die: 0, Revision: 3, Device: 4
int cl_info(void) {
    uint32_t did = DSU_REGS->DSU_DID;  // use header file defines
    uint8_t device    = (did & DSU_DID_DEVSEL_Msk)    >> DSU_DID_DEVSEL_Pos;
    uint8_t revision  = (did & DSU_DID_REVISION_Msk)  >> DSU_DID_REVISION_Pos;
    uint8_t die       = (did & DSU_DID_DIE_Msk)       >> DSU_DID_DIE_Pos;
    uint8_t series    = (did & DSU_DID_SERIES_Msk)    >> DSU_DID_SERIES_Pos;
    uint8_t family    = (did & DSU_DID_FAMILY_Msk)    >> DSU_DID_FAMILY_Pos;
    uint8_t processor = (did & DSU_DID_PROCESSOR_Msk) >> DSU_DID_PROCESSOR_Pos; // Most significant nibble of 32-bit value
    log_msg("Processor: %u, Family: %u, Series: %u, Die: %u, Revision: %u, Device: %u\n",processor, family, series, die, revision, device);
    return 0;
}

int cl_version(void)
{
	log_msg("Version: %s\n",PROJECT_VERSION);
	return 0;
}

void text_in_box(const char *text, const char *color)
{
    if (!text) {
        return;
    }

    // First pass: determine longest line
    size_t max_len = 0;
    const char *p = text;
    const char *line_start = p;

    while (*p) {
        if (*p == '\n') {
            size_t len = (size_t)(p - line_start);
            if (len > max_len) max_len = len;
            line_start = p + 1;
        }
        p++;
    }

    // Handle last line (no trailing \n)
    if (p != line_start) {
        size_t len = (size_t)(p - line_start);
        if (len > max_len) max_len = len;
    }

    // Print top border
    log_msg("\n"); // line-feed BEFORE color sequence
    if(color)log_msg("%s",color);
    log_msg("+");
    for (size_t i = 0; i < max_len + 2; i++)
        log_msg("-");
    log_msg("+");
    if(color)log_msg(COLOR_RESET); // reset color BEFORE line-feed
    log_msg("\n");

    // Second pass: print each line padded
    p = text;
    line_start = p;

    while (1) {
        if (*p == '\n' || *p == 0) {
            size_t len = (size_t)(p - line_start);
            if(color)log_msg("%s",color);
            log_msg("| %.*s", (int)len, line_start);

            // pad remainder
            for (size_t i = len; i < max_len; i++)
                log_msg(" ");

            log_msg(" |");
            if(color)log_msg(COLOR_RESET);
            log_msg("\n");
            if (*p == 0)
                break;

            line_start = p + 1;
        }
        p++;
    }

    // Bottom border
    if(color)log_msg("%s",color);
    log_msg("+");
    for (size_t i = 0; i < max_len + 2; i++)
        log_msg("-");
    log_msg("+");  // lower right corner
    if(color)log_msg(COLOR_RESET); // reset color BEFORE final line-feed
    log_msg("\n");
}
