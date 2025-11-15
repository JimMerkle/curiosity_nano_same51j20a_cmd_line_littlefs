// Copyright Jim Merkle, 10/16/2020
// Module: lfs_interface.c
// Interface code between LittleFS file system and a library for a Winbond w25q128

#include <stdio.h> // printf()
#include <stdlib.h> // malloc()
#include "w25q128.h"
#include "lfs.h"
#include "littlefs/lfs_interface.h" // PARAMETER_NOT_USED
#include "command_line/command_line.h" // argc, argv
#include "logger/logger.h"
#include "definitions.h"                // SYS function prototypes

// Read a region in a block. Negative error codes are propagated
// to the user.
int lfs_read(const struct lfs_config *c, lfs_block_t block,
             lfs_off_t off, void *buffer, lfs_size_t size)
{
  if(!buffer || !size) {
    log_msg("%s Invalid parameter!\r\n",__func__);
    return LFS_ERR_INVAL;
  }
  lfs_block_t address = block * c->block_size + off; 
  //log_msg("+%s(Addr 0x%06X, Len 0x%04X)\r\n",__func__,address,size);
  int rc = W25_ReadData(address, (uint8_t*)buffer, size);
  if(rc != LFS_ERR_OK) {
    log_msg("%s W25_ReadData() error %d\r\n",__func__,rc);
    return LFS_ERR_IO;
  }
  return LFS_ERR_OK; 
}

int lfs_prog(const struct lfs_config *c, lfs_block_t block,
	lfs_off_t off, const void *buffer, lfs_size_t size)
{
  lfs_block_t address = block * c->block_size + off; 
  //log_msg("+%s(Addr 0x%06X, Len 0x%04X)\r\n",__func__,address,size);
  int rc = W25_PageProgram(address, (uint8_t*)buffer, size);
  if(rc != LFS_ERR_OK) {
    log_msg("+%s W25_PageProgram() error\r\n",__func__);
    return rc;
  }
  return rc;   
}

int lfs_erase(const struct lfs_config *c, lfs_block_t block)
{
  lfs_block_t address = block * c->block_size; 
  //log_msg("+%s(Addr 0x%06X)\r\n",__func__,address);
  int rc = W25_SectorErase(address);
  if(rc != LFS_ERR_OK) {
    log_msg("+%s W25_SectorErase() error\r\n",__func__);
    return LFS_ERR_IO;
  }
  return LFS_ERR_OK;
}

int lfs_sync(const struct lfs_config *c)
{
  PARAMETER_NOT_USED(c);
  // write function performs no caching.  No need for sync.
  //log_msg("+%s()\r\n",__func__);
  return LFS_ERR_OK;
  //return LFS_ERR_IO;
}

// variables / structures used by the file system
lfs_t lfs;

#define READ_SIZE               1   // Minimum size of a block read. All read operations will be a multiple of this value.
#define PROGRAM_SIZE            1   // Minimum size of a block program. All program operations will be a multiple of this value.
#define CACHE_SIZE              256 // Used for both read and program buffers
#define LOOKAHEAD_CACHE_SIZE    256

uint8_t read_buffer[CACHE_SIZE];
uint8_t program_buffer[CACHE_SIZE];
uint8_t lookahead_buffer[LOOKAHEAD_CACHE_SIZE];

const struct lfs_config lfs_cfg =
{
    .context = NULL, // not implemented
    .read = lfs_read,   // read function
    .prog = lfs_prog,   // program function
    .erase = lfs_erase,  // erase function
    .sync = lfs_sync,   // sync function
    .read_size = READ_SIZE,              // minimum read size (Our Flash interface supports single byte reads)
    .prog_size = PROGRAM_SIZE,           // Minimum size of a block program. All program operations will be a multiple of this value.
    .block_size = W25_SECTOR_SIZE,       // block_size - 4KByte erase sectors
    .block_count = W25_SECTOR_COUNT,     // block_count - number of sectors        
    .block_cycles = 512,                 // block_cycles - suggested value: 100 - 1000
    .cache_size = CACHE_SIZE,            // cache_size - multiple of read and program block size
    .lookahead_size = LOOKAHEAD_CACHE_SIZE, // lookahead_size (multiple of 8)
    
    .read_buffer = &read_buffer,            // read_buffer
    .prog_buffer = &program_buffer,         // prog_buffer
    .lookahead_buffer = &lookahead_buffer,  // lookahead_buffer

    .name_max = LFS_NAME_MAX,           // name_max
    .file_max = LFS_FILE_MAX,           // file_max
    .attr_max = LFS_ATTR_MAX,           // attr_max
};

// Initialize file system
int lfs_init(void) {
	log_msg("%s: Checking file system...\n",__func__);
    // Test the buffers.  Are they all non-zero?
    if(!lfs_cfg.read_buffer || !lfs_cfg.prog_buffer) {
        log_msg("Buffer problems!! read_buffer: 0x%08lX, program_buffer: 0x%08lX\n",
          (uint32_t)lfs_cfg.read_buffer, (uint32_t)lfs_cfg.prog_buffer);   
        return -1;
    }
    
    // mount the filesystem
    int rc = lfs_mount(&lfs, &lfs_cfg);
    log_msg("lfs_mount - returned: %d\r\n",rc);
    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (rc != LFS_ERR_OK) {
        log_msg("%s: lfs_mount() error, reformatting FS\n",__func__);
        rc = lfs_format(&lfs, &lfs_cfg);
        log_msg("lfs_format - returned: %d\n",rc);
        rc = lfs_mount(&lfs, &lfs_cfg);
        log_msg("lfs_mount - returned: %d\n",rc);
    }
#if 0
    // read current count
    uint32_t boot_count = 55;
    rc = lfs_file_open(&lfs, &file, "boot_count", LFS_O_RDWR | LFS_O_CREAT);
    if(rc != LFS_ERR_OK) log_msg("lfs_file_open - returned: %d\n",err);
    int count = lfs_file_read(&lfs, &file, &boot_count, sizeof(boot_count));
    if(count < 0) log_msg("lfs_file_read - returned: %d\n",err);
    //log_msg("%s() read boot_count: %u\r\n",__func__,boot_count);
    // update boot count
    boot_count += 1;
    //log_msg("%s() writing boot_count: %u\r\n",__func__,boot_count);
    lfs_file_rewind(&lfs, &file);
    lfs_file_write(&lfs, &file, &boot_count, sizeof(boot_count));

    // remember the storage is not updated until the file is closed successfully
    lfs_file_close(&lfs, &file);

    // release any resources we were using
    lfs_unmount(&lfs);

    // print the boot count
    log_msg("boot_count: %u\n", boot_count);
#endif
    //log_msg("-%s()\r\n",__func__);    
    return rc;
}

//=================================================================================================
// Command Line functions that interface with LittleFS
//=================================================================================================

// Display a file system directory
int cl_dir(void)
{
//	InitFlashInterface(); // Make sure QSPI FLASH interface is configured
//    init_lfs_cfg();
//	lfs_mount(&lfs,&lfs_cfg);
	lfs_dir_t dir;
	struct lfs_info info;
	char * directory = (char *)"/";

	// By default (no parameters), display root directory, else, display contents of directory name provided
	if(argc > 1)
		directory = argv[1];

	// Once open, a directory can be used with read to iterate over files.
	// Returns a negative error code on failure.
   //log_msg("calling lfs_dir_open()\n");
	int lfs_status = lfs_dir_open(&lfs, &dir, directory);
	if(LFS_ERR_OK != lfs_status) {
		log_msg("Directory \"%s\" not found\n",directory);
		return lfs_status;
	}

	log_msg("Directory of \"%s\":\n",directory);
	uint32_t total_bytes = 0;
	uint32_t file_count = 0;
	// When iterating through files & directories within a directory, LittleFS will always present
	// "." and ".." as directories within the current directory.
	while(lfs_dir_read(&lfs, &dir, &info) >0) {
		// Directory info update, display data
		if(info.type == LFS_TYPE_DIR) {
          log_msg("<DIR>         %s\n",info.name);
      } else if(info.type == LFS_TYPE_REG) {
          log_msg("%13lu %s\n",info.size,info.name);
          total_bytes += info.size;
          file_count++;
      } // display file info, add to total_bytes, add to file count
		else log_msg("unknown type: %u\n",info.type);
	}
	lfs_dir_close(&lfs, &dir);

	// Display totals and expected space remaining
	// Calculate number of 1024 byte blocks used and subtract from number of blocks allocated for the file system
	lfs_ssize_t blocks_used = lfs_fs_size(&lfs);
	uint32_t bytes_remaining = (W25_SECTOR_COUNT - blocks_used) * W25_SECTOR_SIZE;
	//log_msg("\nFile count: %lu\nBytes total: %lu\nBytes remaining %lu\n",file_count,total_bytes,bytes_remaining);
	log_msg("\nFile count: %lu\nBytes total: %lu\nSectors Used: %lu\nBytes remaining %lu\n",file_count,total_bytes,blocks_used,bytes_remaining);
 	return 0;
}

// Make a directory..  Required 1 argument, the directory name
int cl_make_dir(void)
{
    // verify argument count
    if(argc < 2) {
        log_msg("\nExpect <directory name>\n");
        return 0;
    }

    int retval = lfs_mkdir(&lfs, argv[1]);
    if(retval != LFS_ERR_OK) {
        log_msg("%s: Error creating directory \"%s\"\n",__func__,argv[1]);
        return retval;
    }

    log_msg("Created directory: \"%s\"\n",argv[1]);
    return LFS_ERR_OK;
}

// Remove a file / directory..  Required 1 argument, the directory name
int cl_remove(void)
{
    // verify argument count
    if(argc < 2) {
        log_msg("\nExpect <file/directory name>\n");
        return 0;
    }
    int retval = lfs_remove(&lfs, argv[1]);
    if(retval != LFS_ERR_OK) {
        log_msg("%s: Error removing \"%s\"\n",__func__,argv[1]);
        return retval;
    }

    log_msg("File / directory, \"%s\", removed\n",argv[1]);
    return LFS_ERR_OK;
}

// Make a file..  Required 1 argument, the file name
int cl_make_file(void)
{
    // verify argument count
    if(argc < 2) {
        log_msg("\nExpect <file name>\n");
        return 0;
    }
    lfs_file_t file;
    char buffer[LFS_NAME_MAX]; // to store the stuff we will write to the file

    // Returns a negative error code on failure.
    int retval = lfs_file_open(&lfs, &file,
        argv[1], LFS_O_RDWR | LFS_O_CREAT);

    if(retval != LFS_ERR_OK) {
        log_msg("%s: Error creating file \"%s\"\n",__func__,argv[1]);
        return retval;
    }

    // Let's put the files name into the file
    sprintf(buffer,"Filename: %s",argv[1]);
    int length = strlen(buffer);
    log_msg("Writing: \"%s\", len %d, into file: \"%s\"\n",buffer,length,argv[1]);
    // return value will be number of bytes written to file
    retval = lfs_file_write(&lfs, &file, buffer, length);
    if(retval < LFS_ERR_OK) {
        log_msg("%s: Error writing file \"%s\"\n",__func__,argv[1]);
    }

    // remember the storage is not updated until the file is closed successfully
    // even if error writing, close file before returning
    retval = lfs_file_close(&lfs, &file);
    if(retval != LFS_ERR_OK) {
        log_msg("%s: Error closing file \"%s\"\n",__func__,argv[1]);
    }
    log_msg("Created file: \"%s\"\n",argv[1]);
    return LFS_ERR_OK;
}

// Make a 4KByte file..  Required 1 argument, the file name
// Record the time to create and store the file
int cl_make_file_4kb(void)
{
    // verify argument count
    if(argc < 2) {
        log_msg("\nExpect <file name>\n");
        return 0;
    }
    lfs_file_t file;

    // Returns a negative error code on failure.
    int retval = lfs_file_open(&lfs, &file,
        argv[1], LFS_O_RDWR | LFS_O_CREAT);

    if(retval != LFS_ERR_OK) {
        log_msg("%s: Error creating file \"%s\"\n",__func__,argv[1]);
        return retval;
    }

    // Load the file with incrementing data
    // Create a 256byte buffer with a know data pattern
    uint8_t buf[256];
    for(unsigned i=0;i<sizeof(buf);i++)
    		buf[i] = (uint8_t)i;

    uint32_t start_us = TC0_Timer32bitCounterGet(); // read us hardware timer

    // Write the buffer to the file 16 times
    for(unsigned count=0;count<16;count++) {
		retval = lfs_file_write(&lfs, &file, buf, sizeof(buf));
		if(retval < LFS_ERR_OK) {
			log_msg("%s: Error writing file \"%s\"\n",__func__,argv[1]);
			break;
		}
    }

    // remember the storage is not updated until the file is closed successfully
    // even if error writing, close file before returning
    lfs_file_close(&lfs, &file);

    uint32_t stop_us = TC0_Timer32bitCounterGet(); // read us hardware timer
    if(stop_us < start_us) stop_us += 1<<16; // roll-over, add 16-bit roll-over offset

    log_msg("Created file: \"%s\", Time: %lu us\n",argv[1],stop_us-start_us);
    return retval;
} // cl_make_file_4kb()

void hexdump(void * address, uint32_t count, uint32_t address_value); // hexdump.c

// Display file - Type...  Requires 1 argument, the filename
int cl_cat(void)
{
    // verify argument count
    if(argc < 2) {
        log_msg("\nExpect <file name>\n");
        return 0;
    }
    lfs_file_t file;
    char buffer[120]; // buffer to hold a line+ from the file

    // Returns a negative error code on failure.
    int retval = lfs_file_open(&lfs, &file,
        argv[1], LFS_O_RDONLY);

    if(retval != LFS_ERR_OK) {
        log_msg("%s: Error opening file \"%s\"\n",__func__,argv[1]);
        return retval;
    }
    log_msg("Displaying file \"%s\":\n",argv[1]);
    // Begin looping, loading the line buffer and displaying text from it,
    //  until we have displayed all the text from the file.
    int bytesread;
    char c;
    do {
        bytesread = lfs_file_read(&lfs, &file, buffer, sizeof(buffer));
        if(bytesread>=0) {
            //log_msg("%s: bytesread: %d\n",__func__,bytesread);
            //hexdump(buffer, bytesread, 0);
        } else {
            log_msg("%s: Error reading file \"%s\"\n",__func__,argv[1]);
            break; // exit outer while-loop
        }
        int i=0; // index within line buffer
        while(i<bytesread) {
            c = buffer[i++]; // post increment
            if((c >= ' ' && c <= '~') || c=='\r' || c=='\n' || c=='\t')
                log_msg("%c",c);
            // for now, ignore everything else
        } // display the characters held in buffer
        SYSTICK_DelayMs(12); // 120 characters * 10 USART bits / char / 115200 = 10.4ms
    } while(bytesread); // keep looping as long as we keep getting data from file

    // Close file before returning
    lfs_file_close(&lfs, &file);
    log_msg("\n\n");

    return LFS_ERR_OK;
}

// Display file data as hexadecimal - Requires 1 argument, the filename
int cl_file_dump(void)
{
    // verify argument count
    if(argc < 2) {
        log_msg("\nExpect <file name>\n");
        return 0;
    }
    lfs_file_t file;
    uint32_t file_offset = 0;
    char buffer[512]; // buffer to hold file data

    // Returns a negative error code on failure.
    int retval = lfs_file_open(&lfs, &file,
        argv[1], LFS_O_RDONLY);

    if(retval != LFS_ERR_OK) {
        log_msg("%s: Error opening file \"%s\"\n",__func__,argv[1]);
        return retval;
    }
    log_msg("Displaying file \"%s\":\n",argv[1]);
    // Begin looping, loading the file buffer and displaying hex data from it,
    //  until we have displayed the whole file.
    int bytesread;
    do {
        bytesread = lfs_file_read(&lfs, &file, buffer, sizeof(buffer));
        if(bytesread < LFS_ERR_OK) {
            log_msg("%s: Error reading file \"%s\"\n",__func__,argv[1]);
        }
    	// Use our favorite Hex Dump routine to display the memory
        hexdump(buffer, bytesread, file_offset); // hexdump.c
        file_offset += bytesread; // update offset for next pass
        SYSTICK_DelayMs(225); // 512 characters / 16 characters per line * 80 characters / line * 10 USART bits / char / 115200 = 222.2ms
    } while(bytesread); // keep looping as long as we keep getting data from file

    // Close file before returning
    lfs_file_close(&lfs, &file);
    log_msg("\n\n");

    return LFS_ERR_OK;
}

// Copy file - Source - Destination
int cl_copy(void)
{
    // verify argument count
    if(argc < 3) {
        log_msg("\nExpect <source file> <destination file>\n");
        return 0;
    }
    lfs_file_t source;
    lfs_file_t destination;
    char buffer[1024]; // buffer to copy data

    // Open source file, read only
    // Returns a negative error code on failure.
    int32_t retval = lfs_file_open(&lfs, &source, argv[1], LFS_O_RDONLY);
    if(retval != LFS_ERR_OK) {
        log_msg("%s: Error opening source file \"%s\", %ld\n",__func__,argv[1],retval);
        return retval;
    }

    // Open destination file, write only, file must not already exist
    // Returns a negative error code on failure.
    retval = lfs_file_open(&lfs, &destination, argv[2], LFS_O_WRONLY | LFS_O_CREAT | LFS_O_EXCL);
    if(retval != LFS_ERR_OK) {
        log_msg("%s: Error opening destination file \"%s\", %ld\n",__func__,argv[2],retval);
        // Close source file
        lfs_file_close(&lfs, &source);
        return retval;
    }

    // Both files are open and ready to begin the copy process
    int32_t bytes_read;

    do {
    	// read data from source file
        bytes_read = lfs_file_read(&lfs, &source, buffer, sizeof(buffer));
        if(bytes_read < LFS_ERR_OK) {
            log_msg("%s: Error reading file \"%s\"\n",__func__,argv[1]);
        	break; // exit do-while loop
        }
        // write data to destination file
		retval = lfs_file_write(&lfs, &destination, buffer, bytes_read);
		if(retval < LFS_ERR_OK) {
			log_msg("%s: Error writing file \"%s\"\n",__func__,argv[2]);
			break;
		}
    } while(bytes_read); // continue looping while lfs_file_read() returns data

    // Close files before returning
    lfs_file_close(&lfs, &source);
    lfs_file_close(&lfs, &destination);
    log_msg("\n\n");

    return LFS_ERR_OK;
} // cl_copy()


// Rename a file / directory.  If directory, it must be empty.
// This command requires 2 command line arguments, <current name> <new name>
int cl_rename(void)
{
    // verify argument count
    if(argc < 3) {
        log_msg("\nExpect <original name> <new name>\n");
        return 0;
    }
    // Returns a negative error code on failure.
    int retval = lfs_rename(&lfs, argv[1], argv[2]);

    if(retval != LFS_ERR_OK) {
        log_msg("Unable to rename \"%s\" to \"%s\"\n",argv[1],argv[2]);
        log_msg("If directory, it MUST be empty!\n");
        return retval;
    }

    log_msg("Rename success\n");
    return retval;
} // cl_rename()

// Assume timer 4 is enabled and configured to clock at 1us (1MHz) rate
// This command requires 1 command line argument, <file name>
// The time it takes to open the file, read the file, and close the file will be measured and reported.
int cl_readspeed(void)
{
    // verify argument count
    if(argc < 2) {
        log_msg("\nExpect <file name>\n");
        return 0;
    }
    lfs_file_t file;
    uint32_t start_us = TC0_Timer32bitCounterGet(); // read us hardware timer

    // Returns a negative error code on failure.
    int retval = lfs_file_open(&lfs, &file,
        argv[1], LFS_O_RDONLY);

    if(retval != LFS_ERR_OK) {
        log_msg("%s: Error opening file \"%s\"\n",__func__,argv[1]);
        return retval;
    }

    int bytes_read = 0;
    int total_bytes_read = 0;
    uint8_t buf[1024];
    // Loop, reading the file into a 1K buffer, until the entire file has been read
    do {
    	bytes_read = lfs_file_read(&lfs, &file, buf, sizeof(buf));
        if(bytes_read < LFS_ERR_OK) {
            log_msg("%s: Error reading file \"%s\"\n",__func__,argv[1]);
            break; // done reading
        }
        total_bytes_read += bytes_read;

    } while(bytes_read > 0); // keep looping as long as we keep getting data from file

    lfs_file_close(&lfs, &file);

    uint32_t stop_us = TC0_Timer32bitCounterGet(); // read us hardware timer
    if(stop_us < start_us) stop_us += 1<<16; // roll-over, add 16-bit roll-over offset

    log_msg("Read file: \"%s\", Time: %lu us\n",argv[1],stop_us-start_us);
    return retval;
} // cl_readspeed()


#if 0
// Read line of text from file into single character buffer until newline (LF) or EOF.
// Includes newline if space permits. Always null-terminates.
// Returns >0 for number of characters read
// Returns 0 if EOF and no characters read
// Returns <0 for errors
int lfs_gets(lfs_file_t *file, char *buffer, size_t buffsize) {
    size_t count = 0;
    char c;

    if (buffsize == 0) {
        return -1; // invalid buffer size
    }

    while (count + 1 < buffsize) {
        int lfs_ret = lfs_file_read(&lfs, file, &c, 1);
        if (lfs_ret == 0) {
            // EOF
            break;
        } else if (lfs_ret < 0) {
            return lfs_ret; // Error
        }

        buffer[count++] = c;
        if (c == '\n') {
            break; // stop at newline
        }
    }

    buffer[count] = '\0'; // always terminate

    return (count > 0) ? (int)count : 0;
} //lfs_gets()

#else

// fgets-like function using LittleFS with client buffer.
// Reads up to buffsize-1 characters into buffer, stopping at newline or EOF.
// Always null-terminates. Returns:
//   >0 : number of characters read
//    0 : EOF and no characters read
//   <0 : error code from lfs_file_read
int lfs_gets(lfs_file_t *file, char *buffer, size_t buffsize) {
    if (buffer == NULL ||buffsize == 0) {
        return -1; // invalid buffer address or size
    }
     buffer[0] = '\0'; // ensure null-termination on error
    // Read up to buffsize-1 bytes directly into buffer
    int lfs_ret = lfs_file_read(&lfs, file, buffer, buffsize - 1);
    if (lfs_ret == 0) {
        // EOF
        return 0;
    } else if (lfs_ret <= 0) {
        // EOF or Error
        return lfs_ret;
    }

    // Scan for newline
    int count = 0;
    while (count < lfs_ret) {
        if (buffer[count] == '\n') {
            // Found newline → terminate after it
            buffer[count + 1] = '\0';

            // Adjust file pointer back to just after newline
            // We read lfs_ret bytes, but only consumed count+1
            lfs_file_seek(&lfs, file, -(lfs_ret - (count + 1)), LFS_SEEK_CUR);
            return count + 1;
        }
        count++; // advance to next character
    }

    // No newline found -> terminate at end
    buffer[count] = '\0';
    return count;
}
#endif



// Write 1MByte of file data to 4K byte files.  Record time.  Display count after each file is written.
// Requires 1 argument, the base part of file names
// Record the time to create and store the files
int cl_make_files_1mb(void)
{
    // verify argument count
    if(argc < 2) {
        log_msg("\nExpect <file name>\n");
        return 0;
    }
    lfs_file_t file;
    uint32_t bytes_written = 0;
    uint32_t stop_count = 0x100000;
    int rc;
    log_msg("This will take a little while... Creating 256 - 4KByte files...\n");

    // Load each file with incrementing data
    // Create a 4096 byte buffer with known data pattern
    uint8_t buf[4096];
    for(unsigned i=0;i<sizeof(buf);i++)
    	buf[i] = (uint8_t)i;

    uint32_t start_ms = SYSTICK_GetTickCounter(); // read ticks

    for(int file_name_counter=1;bytes_written<stop_count;file_name_counter++) {
    	char filename[64];
    	sprintf(filename,"%s_%d",argv[1],file_name_counter);

		// Returns a negative error code on failure.
		rc = lfs_file_open(&lfs, &file, filename, LFS_O_RDWR | LFS_O_CREAT);
		if(rc != LFS_ERR_OK) {
			log_msg("%s: Error creating file \"%s\"\n",__func__,filename);
			return rc;
		}

		// Write the buffer to the file
		rc = lfs_file_write(&lfs, &file, buf, sizeof(buf));
		if(rc < LFS_ERR_OK) {
			log_msg("%s: Error writing file \"%s\"\n",__func__,filename);
			return rc;
		}

		// remember the storage is not updated until the file is closed successfully
		// even if error writing, close file before returning
		lfs_file_close(&lfs, &file);
		// Display time to write first file
	    if(file_name_counter==1) log_msg("Time per file: %lu ms\n",SYSTICK_GetTickCounter() - start_ms);

	    // With 1024 files being created, output counter for each file
		log_msg("%d\n",file_name_counter);
		bytes_written += sizeof(buf);
    }
    uint32_t stop_ms = SYSTICK_GetTickCounter(); // read Ticks (milliseconds)
    log_msg("Created 256 files in %lu ms\n",stop_ms-start_ms);
    return rc;
} // cl_make_files1_1mb()
