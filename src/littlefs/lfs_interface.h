/*
 * lfs_interface.h
 *
 *  Created on: May 26, 2022
 *      Author: Jim Merkle
 *
 *  Using STM32 HAL interface modules, create an interface between the LittleFS file system
 *  and an STM32's unused FLASH Program Memory
 *  In this example, we will use a fixed FLASH block size, with a fixed address.
 */
#ifndef _lfs_interface_h_
#define _lfs_interface_h_

#include "w25q128.h"

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/** This macro is used to suppress compiler messages about a parameter not being used in a function. */
#define PARAMETER_NOT_USED(p) (void) ((p))

int lfs_init(void); // Initialization for LittleFS

// Command Line functions implemented within littlefs_interface.c:
int cl_lfs(void);
int cl_dir(void);
int cl_make_dir(void);
int cl_remove(void);
int cl_rmdir(void);
int cl_make_file(void);
int cl_make_file_4kb(void);
int cl_rename(void);
int cl_cat(void);
int cl_copy(void);
int cl_file_dump(void);
int cl_readspeed(void);
int cl_make_files_1mb(void);

// Records to add into command line interface (command_line.c):
#define LITTLEFS_COMMANDS \
{"dir",        "Directory listing for file system",                         cl_dir}, \
{"mkdir",      "Make Directory",                                            cl_make_dir}, \
{"remove",     "Remove File/Directory (directory must be empty)",           cl_remove}, \
{"makefile",   "Make a file <file name>",                                   cl_make_file}, \
{"makefile4k", "Make a 4K byte file <file name>",                           cl_make_file_4kb}, \
{"rename",     "Rename file/directory <current name> <new name>",           cl_rename}, \
{"cat",        "Display text file (only printable text)",                   cl_cat}, \
{"type",       "Display text file (only printable text)",                   cl_cat}, \
{"copy",       "Copy file <source file name> <destination file name>",      cl_copy}, \
{"filedump",   "Display <file> data as Hexadecimal",                        cl_file_dump}, \
{"readspeed",  "Display time to open, read, and close <file>",              cl_readspeed}, \
{"1mbfiles",   "Create 256 - 4KByte files (1MByte) with <basename>",        cl_make_files_1mb} \

#endif // _lfs_interface_h_
