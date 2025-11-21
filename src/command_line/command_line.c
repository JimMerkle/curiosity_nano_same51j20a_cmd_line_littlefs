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

// Typedefs
typedef struct {
  char * command;
  char * comment;
  int (*function)(void); // pointer to command function
} COMMAND_ITEM;

const COMMAND_ITEM cmd_table[] = {
    {"?",         "display help menu",                                      cl_help},
    {"help",      "display help menu",                                      cl_help},
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
    // Turn on yellow text, print greeting, reset attributes
    log_msg("\n" COLOR_YELLOW "Command Line parser, Version: %s, Date: %s" COLOR_RESET "\n",PROJECT_VERSION,__DATE__);
    log_msg(COLOR_YELLOW "Enter \"help\" or \"?\" for list of commands" COLOR_RESET "\n");
    log_msg(">"); // initial prompt
}

// Check for data available from USART interface.  If none present, just return.
// If data available, process it (add it to character buffer if appropriate)
void cl_loop(void)
{
    static int index = 0; // index into global buffer
    int c;

    // Spin, reading characters until EOF character is received (no data), buffer is full, or
    // a <line feed> character is received.  Null terminate the global string, don't return the <LF>
    while(1) {
      c = __io_getchar();
      switch(c) {
          case EOF:
              return; // non-blocking - return
          case _CR:
          case _LF:
            buffer[index] = 0; // null terminate
            if(index) {
        		log_msg("\n"); // newline
            	cl_process_buffer(); // process the null terminated buffer
            }
    		log_msg("\n>");
            index = 0; // reset buffer index
            return;
          case _BS:
            if(index<1) continue;
            log_msg("\b \b"); // remove the previous character from the screen and buffer
            index--;
            break;
          default:
        	if(index<(MAXSERIALBUF - 1) && c >= ' ' && c <= '~') {
                log_msg("%c",c); // write character to terminal
                buffer[index] = (char) c;
                index++;
        	}
      } // switch
  } // while(1)
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

//#include "same51j20a.h"
//#include "core_cm4.h"  // CMSIS header for Cortex-M4
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

