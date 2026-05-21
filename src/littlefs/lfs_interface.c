// Copyright Jim Merkle, 10/16/2020
// Module: lfs_interface.c
// Interface code between LittleFS file system and a library for a Winbond w25q128

#include <stdio.h> // printf()
#include <stdlib.h> // malloc()
#include <string.h>
#include <string.h>
#include <string.h>
#include "w25q128.h"
#include "lfs.h"
#include "littlefs/lfs_interface.h" // PARAMETER_NOT_USED
#include "command_line/command_line.h" // argc, argv
#include "logger/logger.h"
#include "definitions.h"                // SYS function prototypes

#if 0
typedef struct {
   uint32_t erase_start;      // Enter function
   uint32_t write_enable;     // Enable complete
   uint32_t erase_command;    // Command complete
   uint32_t busy_poll;        // Polling complete (leaving function)
} ERASE_TIMING;

#define FLASH_TIME_ARRAY_SIZE 50
// Array to hold FLASH Erase value
ERASE_TIMING flash_erase_times[FLASH_TIME_ARRAY_SIZE] = {0};
//uint32_t flash_program_times[FLASH_TIME_ARRAY_SIZE] = {0};
int erase_count = 0;
//int program_count = 0;
#endif

#if 0
// Older original code using the W25Q128 functions
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
#endif

// "New Read" function
// Read a region of SPI FLASH memory into buffer.
// Negative error codes are propagated to the user.
int lfs_read(const struct lfs_config *c, lfs_block_t block,
             lfs_off_t off, void *buffer, lfs_size_t size)
{
   uint32_t address = block * c->block_size + off; 
   
   // 1. Activate chip select
   W25_CS_ENABLE();
   
   // 2. Create and issue read command
   uint8_t cmd[4] = {W25_CMD_READ_DATA,address>>16,address>>8,address};
   SERCOM1_SPI_Write(cmd , sizeof(cmd));
   
   // 3. Read in the payload
   SERCOM1_SPI_Read(buffer, size);
   
   // 4. Deactivate chip select
   W25_CS_DISABLE();

   return LFS_ERR_OK;
}

// page program and flash erase helper function
// assumes the chip select is active, and the calling routine
// is waiting for status register 1 busy bit to clear
// returns: 0:success, 1:timeout
uint8_t poll_busy_until_clear(uint32_t timeout) {
   uint32_t elapsed_ms;
   uint32_t entry_ms = SYSTICK_GetTickCounter();

   // Poll status register1 until BUSY clears or elapsed time exceeds timeout
   uint8_t status;
   uint8_t rdsr1 = W25_CMD_READ_STATUS_REG_1;
   do {
      // 1. Activate chip select
      W25_CS_ENABLE();
      
      // 2. Issue SPI write and read functions to read in status1
      SERCOM1_SPI_Write(&rdsr1, 1);
      SERCOM1_SPI_Read(&status, 1);
      
      // 3. Deactivate chip select
      W25_CS_DISABLE(); 
      
      status &= W25_STATUS1_BUSY;
      elapsed_ms = SYSTICK_GetTickCounter() - entry_ms;
   } while ( status && (elapsed_ms < timeout)); // loop while busy and not timeout
   //log_msg("%s: elapsed_ms: %lu\n",__func__,elapsed_ms);
   return status;
}

// New page program function
// Winbond 8.2.15 Page Program (02h)
// Write one byte up to 256 bytes (a page) of data (original comment - now supports multiple page writes)
// This function has been upgraded to write multiple pages, using multiple page write commands.
// LittleFS is unaware of the page boundary issue.  Manage the issue with multiple writes
int lfs_prog(const struct lfs_config *c, lfs_block_t block,
             lfs_off_t off, const void *buffer, lfs_size_t size)
{
   uint32_t address = block * c->block_size + off;
   uint8_t *src = (uint8_t*)buffer;

   // 1. Issue write enable command
   W25_CS_ENABLE();
   uint8_t cmd = W25_CMD_WRITE_ENABLE;
   SERCOM1_SPI_Write(&cmd, 1);
   W25_CS_DISABLE();
   
   while (size > 0) {
      // Calculate chunk size within current 256-byte page
      uint32_t page_offset = address % W25_PROGRAM_PAGE_SIZE;
      uint32_t chunk = W25_PROGRAM_PAGE_SIZE - page_offset;
      if (chunk > size) chunk = size;
      
      // 2. Activate chip select
      W25_CS_ENABLE();

      // 3. Create and issue page program command
      uint8_t cmd[4] = {W25_CMD_PAGE_PROGRAM,address>>16,address>>8,address};
      SERCOM1_SPI_Write(cmd, sizeof(cmd));

      // 3. Write data payload
      SERCOM1_SPI_Write(src, chunk);
      
      // 4. Deactivate chip select
      W25_CS_DISABLE();     

      // 5) Poll BUSY until clear (with timeout)
      uint8_t busy = poll_busy_until_clear(PAGE_PROGRAM_TIMEOUT);
      if (busy) {
         //log_msg("%s timeout waiting for program at 0x%06lX\n", __func__, (unsigned long)address);
         return LFS_ERR_IO;
      }

      // Advance
      address += chunk;
      src     += chunk;
      size    -= chunk;
   }

   return LFS_ERR_OK;
}

// New Erase function
int lfs_erase(const struct lfs_config *c, lfs_block_t block)
{
   //if(erase_count<FLASH_TIME_ARRAY_SIZE) {flash_erase_times[erase_count].erase_start = DWT_CounterGet();}
   uint32_t address = block * c->block_size;
   
   // 1. Issue write enable command
   // 2. Activate chip select
   W25_CS_ENABLE();
   uint8_t we = W25_CMD_WRITE_ENABLE;
   SERCOM1_SPI_Write(&we, 1);
   // 3. Deactivate chip select
   W25_CS_DISABLE();
   //if(erase_count<FLASH_TIME_ARRAY_SIZE) {flash_erase_times[erase_count].write_enable = DWT_CounterGet();}   
   // 4. Activate chip select
   W25_CS_ENABLE();   
   
   // 5. Create and issue sector erase command sequence
   uint8_t cmd[4] = {W25_CMD_SECTOR_ERASE,address>>16,address>>8,address};
   SERCOM1_SPI_Write(cmd , sizeof(cmd));
   
   // 6. Deactivate chip select
   W25_CS_DISABLE();
   //if(erase_count<FLASH_TIME_ARRAY_SIZE) {flash_erase_times[erase_count].erase_command = DWT_CounterGet();} 
   // 7. Poll status register1 until BUSY clears
   poll_busy_until_clear(SECTOR_ERASE_TIMEOUT);
   
   //if(erase_count<FLASH_TIME_ARRAY_SIZE) {flash_erase_times[erase_count].busy_poll = DWT_CounterGet();} 
   //erase_count++;
   
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

   //.read_buffer = &read_buffer,            // read_buffer
   //.prog_buffer = &program_buffer,         // prog_buffer

   .read_buffer = NULL,                // allow lfs allocate per file
   .prog_buffer = NULL,

   .lookahead_buffer = NULL,           // lookahead_buffer

   .name_max = LFS_NAME_MAX,           // name_max
   .file_max = LFS_FILE_MAX,           // file_max
   .attr_max = LFS_ATTR_MAX,           // attr_max
};

uint32_t boot_count = 0;   // global accessible by other clients

#if BOOT_COUNT_ENABLED
int update_bootcount(void) {
    const char boot_count_name[] = "boot_count.txt";
    lfs_file_t file;
    char buf[16]; // enough for "4294967295\0"

    // Open or create the file
    int rc = lfs_file_open(&lfs, &file, boot_count_name,
                           LFS_O_RDWR | LFS_O_CREAT);
    if (rc < 0) {
        log_msg("%s: lfs_file_open failed rc=%d\n", __func__, rc);
        return rc;
    }

    // Read existing string
    int count = lfs_file_read(&lfs, &file, buf, sizeof(buf)-1);
    if (count < 0) {
        log_msg("%s: lfs_file_read failed rc=%d\n", __func__, count);
        lfs_file_close(&lfs, &file);
        return count;
    }
    buf[count] = '\0'; // ensure null termination

    // Convert to integer
    if (count > 0) {
        boot_count = (uint32_t)strtoul(buf, NULL, 10);
    } else {
        boot_count = 0; // file was empty
    }

    // Increment
    boot_count++;

    // Rewind and overwrite with new string
    lfs_file_rewind(&lfs, &file);
    snprintf(buf, sizeof(buf), "%u", boot_count);
    rc = lfs_file_write(&lfs, &file, buf, strlen(buf));
    if (rc < 0) {
        log_msg("%s: lfs_file_write failed rc=%d\n", __func__, rc);
        lfs_file_close(&lfs, &file);
        return rc;
    }

    // Close file to commit changes
    lfs_file_close(&lfs, &file);

    // Print the boot count
    log_msg("boot_count: %u\n", boot_count);

    return 0;
}
#endif

// Initialize LittleFS: try mount up to 5 times with backoff,
// then format once if mount fails. Return 0 on success, <0 on error.
int lfs_init(lfs_t *lfs, const struct lfs_config *cfg) {
    //log_msg("%s: Checking file system...\n",__func__);
    int rc;
    uint32_t backoff = 10; // initial backoff in ms
    
    // Initial delay following reset
    SYSTICK_DelayMs(50);

    // Try mounting up to 5 times
    for (int attempt = 1; attempt <= 5; attempt++) {
        rc = lfs_mount(lfs, cfg);
        if (rc == 0) {
            //log_msg("lfs_init: mount succeeded on attempt %d\n", attempt);
#if BOOT_COUNT_ENABLED
            update_bootcount();
#endif
            return 0;
        }
        log_msg("lfs_init: mount attempt %d failed (rc=%d)\n", attempt, rc);

        // Backoff before next attempt
        SYSTICK_DelayMs(backoff);
        backoff *= 2; // exponential backoff
    }

    // If all mount attempts failed, try format once
    log_msg("lfs_init: formatting filesystem...\n");
    rc = lfs_format(lfs, cfg);
    if (rc != 0) {
        log_msg("lfs_init: format failed (rc=%d)\n", rc);
        return rc; // unrecoverable error
    }

    // After format, try mount once more
    rc = lfs_mount(lfs, cfg);
    if (rc != 0) {
        log_msg("lfs_init: mount after format failed (rc=%d)\n", rc);
        return rc;
    }

    //log_msg("lfs_init: format + mount succeeded\n");
#if BOOT_COUNT_ENABLED
    update_bootcount();
#endif
    
    return 0;
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
   // lfs_dir_read() returns number of entries read)
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


/* Comment to disable wildcard remove support in cl_remove()
    Example patterns: "*.txt", "log?", "subdir/*.bin" */
#define WILD_REMOVE 1

#ifdef WILD_REMOVE
/* Simple wildcard matcher supporting '*' and '?' */
static int wildcard_match(const char *pattern, const char *str) {
    const char *s = str;
    const char *p = pattern;
    const char *star = NULL;
    const char *ss = NULL;

    while (*s) {
        if (*p == '*') {
            while (*p == '*') p++; /* collapse multiple '*' */
            if (!*p) return 1; /* trailing '*' matches rest */
            star = p;
            ss = s;
            continue;
        }
        if (*p == '?' || *p == *s) {
            p++; s++;
            continue;
        }
        if (star) {
            /* backtrack: advance match in s */
            ss++;
            s = ss;
            p = star;
            continue;
        }
        return 0;
    }
    while (*p == '*') p++;
    return (*p == '\0');
}
#endif /* WILD_REMOVE */


// Remove a file / directory..  Required 1 argument, the directory name
int cl_remove(void)
{
    // verify argument count
    if(argc < 2) {
        log_msg("\nExpect <file/directory name>\n");
        return 0;
    }
    const char *arg = argv[1];

#ifdef WILD_REMOVE
    /* If argument contains wildcard characters, perform directory scan & remove matches */
    const char *p = arg;
    int has_wild = 0;
    while (*p) { if (*p == '*' || *p == '?') { has_wild = 1; break; } p++; }

    if (has_wild) {
        /* Split into directory and pattern parts (e.g. "subdir/*.bin") */
        const char *slash = strrchr(arg, '/');
        char dirbuf[LFS_NAME_MAX+2];
        const char *dirpath;
        const char *pattern;

        if (slash) {
            size_t dirlen = slash - arg;
            if (dirlen == 0) {
                dirpath = "/";
            } else {
                if (dirlen >= sizeof(dirbuf)) dirlen = sizeof(dirbuf)-1;
                strncpy(dirbuf, arg, dirlen);
                dirbuf[dirlen] = '\0';
                dirpath = dirbuf;
            }
            pattern = slash + 1;
        } else {
            dirpath = "/"; /* search root when no directory given */
            pattern = arg;
        }

        lfs_dir_t d;
        struct lfs_info info;
        int rc = lfs_dir_open(&lfs, &d, dirpath);
        if (rc != LFS_ERR_OK) {
            log_msg("Directory \"%s\" not found\n", dirpath);
            return rc;
        }

        while (lfs_dir_read(&lfs, &d, &info) > 0) {
            if (info.type == LFS_TYPE_REG || info.type == LFS_TYPE_DIR) {
                if (wildcard_match(pattern, info.name)) {
                    char fullpath[LFS_NAME_MAX + 32];
                    if (strcmp(dirpath, "/") == 0) {
                        snprintf(fullpath, sizeof(fullpath), "%s", info.name);
                    } else {
                        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, info.name);
                    }
                    int r = lfs_remove(&lfs, fullpath);
                    if (r != LFS_ERR_OK) {
                        log_msg("%s: Error removing \"%s\" rc=%d\n", __func__, fullpath, r);
                    } else {
                        log_msg("Removed: \"%s\"\n", fullpath);
                    }
                }
            }
        }
        lfs_dir_close(&lfs, &d);
        return LFS_ERR_OK;
    }
#endif /* WILD_REMOVE */

    /* No wildcard: remove single file/dir as before */
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

// Make a file..
// Record the time to create and store the file
int make_file(const char * filename, uint32_t filesize)
{
    lfs_file_t file;

    // Returns a negative error code on failure.
    int retval = lfs_file_open(&lfs, &file,
        filename, LFS_O_RDWR | LFS_O_CREAT);

    if(retval != LFS_ERR_OK) {
        log_msg("%s: Error creating file \"%s\"\n",__func__,filename);
        return retval;
    }

    // Load the file with incrementing data
    // Create a 256byte buffer with a know data pattern
    uint8_t buf[256];
    for(unsigned i=0;i<sizeof(buf);i++)
    		buf[i] = (uint8_t)i;

    uint32_t start_us = TC0_Timer32bitCounterGet(); // read us hardware timer

    // Determine how many 256 byte writes will be required
    unsigned write_loops = filesize / sizeof(buf);
    // Write the buffer to the file 16 times
    for(unsigned count=0;count<write_loops;count++) {
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

    log_msg("Created file: \"%s\", Size: %u, Time: %lu us\n",filename,filesize,stop_us-start_us);
    return retval;
} // make_file()

// Make a 4KByte file..  Required 1 argument, the file name
// Record the time to create and store the file
int cl_make_file_4kb(void) {
    // verify argument count
    if(argc < 2) {
        log_msg("\nExpect <file name>\n");
        return 0;
    }
    return make_file(argv[1], 0x1000);
} // cl_make_file_4kb()

// Make a 1MByte file..  Required 1 argument, the file name
// Record the time to create and store the file
int cl_make_file_1mb(void) {
    // verify argument count
    if(argc < 2) {
        log_msg("\nExpect <file name>\n");
        return 0;
    }
    return make_file(argv[1], 0x100000);
} // cl_make_file_1mb()

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


#if 0
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
#endif

// Assuming a 120MHz counter is used, convert 120MHz count into a us string with decimal point
// Example: Convert 6552 counts to 54.6us
// Convert counts at 120 MHz into "X.Y" micro-second and tenths of microsecond parts
#define FORMAT_US(counts) ((counts)/12/10), ((counts)/12%10)

#if 0
int cl_dump_sector_erase_times(void) {
   log_msg("%+s\n", __func__);
   for (int i = 0; i < FLASH_TIME_ARRAY_SIZE; i++) {
      if (!flash_erase_times[i].erase_start) break;

      log_msg("i: %02d  WE: %u.%u us, EC: %u.%u us, BP: %u.%u us, Total: %u.%u us\n",
           i,
           FORMAT_US(flash_erase_times[i].write_enable - flash_erase_times[i].erase_start),
           FORMAT_US(flash_erase_times[i].erase_command - flash_erase_times[i].write_enable),
           FORMAT_US(flash_erase_times[i].busy_poll - flash_erase_times[i].erase_command),
           FORMAT_US(flash_erase_times[i].busy_poll - flash_erase_times[i].erase_start));
   }
   return 0;
}
#endif
