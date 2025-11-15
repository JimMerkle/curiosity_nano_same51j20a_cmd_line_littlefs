// Copyright Jim Merkle, 2/17/2020
// Module: hexdump.c
//
// Hexadecimal memory / register display routine

// #include <stdio.h> // printf()
#include <stdlib.h> // strtol()
#include <stdint.h>
#include "logger/logger.h"              // use log_msg()
#include "command_line/command_line.h"  // argv

// Here's the output I want from a hexdump(address, 23) routine:
//00000000  02 03 1f 00 0d 00 00 00  31 32 33 00 00 00 00 00  |........123.....|
//00000010  00 00 00 00 00 00 00                              |.......|
// "address_value" is the number starting each line of the dump.
// This may be actual memory address or file offset
void hexdump(void * address, uint32_t count, uint32_t address_value)
{
	uint8_t* data = (uint8_t*)address;
	int32_t remaining = count;
	uint8_t i;
	while(remaining > 0) {
		uint8_t bytes_this_line = remaining < 16? remaining:16; // use 16 or smaller value
		// Start by displaying the address for this line
		log_msg("%08lX  ",(uint32_t)address_value);
        // Display a row of data
        for(i=0;i<bytes_this_line;i++)
            log_msg("%02X %s",data[i],i==7?(char*)" ":(char*)""); // print hex byte, and if needed, the gap between two groups of 8 bytes
        // If we didn't display a complete line, write spaces
        if(bytes_this_line < 16) {
            for(i=bytes_this_line;i<16;i++)
                log_msg("   ");
            if(bytes_this_line < 8)
                log_msg(" "); // add another space for that little gap
        }

        // Add some space before ASCII data display, and the starting '|'
        log_msg("  |");
        // Display ASCII
        for(i=0;i<bytes_this_line;i++) {
            if(data[i] >= ' ' && data[i] <= '~')
                log_msg("%c",(char)data[i]); // printable ascii
            else
                log_msg(".");  // non-printable
        } // for-loop

        // Print ending '|' and new line
        log_msg("|\n");

        // Line by line updates
        data += bytes_this_line;
        address_value += bytes_this_line;
        remaining -= bytes_this_line;
    }
    // Add an additional line feed before returning
    //log_msg("\n");
} // hexdump()

void memory_dump(void * address, uint32_t count)
{
	return hexdump(address, count, (uint32_t) address);
}

void file_dump(void * address, uint32_t count)
{
	return hexdump(address, count, 0);
}



// Command Line interface to hexdump() routine
// Need to add the following line to the cmd_table in command_line.c:
// {"dump", "dump <address> <count - optional>", 2, cl_dump},
int cl_dump(void)
{
	uint32_t address = strtol(argv[1],NULL,0); // allow user to use decimal or hex
	uint32_t count = 0x100;
	// Set default count to 256 bytes, else allow client to over-ride
	if(argc > 2)
		count = strtol(argv[2],NULL,0); // allow user to use decimal or hex

	memory_dump((void *) address,count);
	return 0;
}
