// Copyright Jim Merkle, 10/10/2020
// w25q128.h
// Author: Jim Merkle, 10/10/2020

#ifndef _w25q128_h_
#define _w25q128_h_

#include <stdint.h> // uint8_t

#define W25_CMD_WRITE_ENABLE        0x06
#define W25_CMD_WRITE_DISABLE       0x04
#define W25_CMD_READ_STATUS_REG_1   0x05
#define W25_CMD_READ_JEDEC_ID       0x9F
#define W25_CMD_READ_UNIQUE_ID      0x4B
#define W25_CMD_READ_DATA           0x03
#define W25_CMD_PAGE_PROGRAM        0x02
#define W25_CMD_SECTOR_ERASE        0x20
#define W25_CMD_CHIP_ERASE          0x60

// Status Register 1 bits (see section 7.1 Status Registers)
#define W25_STATUS1_BUSY            1<<0
#define W25_STATUS1_WEL             1<<1
#define W25_STATUS1_BP0             1<<2
#define W25_STATUS1_BP1             1<<3
#define W25_STATUS1_BP2             1<<4
#define W25_STATUS1_TB              1<<5
#define W25_STATUS1_SEC             1<<6
#define W25_STATUS1_SRP0            1<<7

#define TIMEOUT                 100 // MS Timeout for HAL function calls
#define PAGE_PROGRAM_TIMEOUT    1000 // MS Timeout for Program
#define SECTOR_ERASE_TIMEOUT    1000 // MS Timeout for Sector Erase, Program, and Chip erase
#define CHIP_ERASE_TIMEOUT     60000 // MS Timeout for Chip erase

#define W25_JEDEC_ID_BUF_SIZE   3   // bytes
#define W25_UNIQUE_ID_BUF_SIZE  8   // bytes
#define W25_PROGRAM_BLOCK_SIZE  256  // bytes - can write from 1 up to 256 bytes
#define W25_SECTOR_SIZE         4096 // bytes - used for erasing portions device
#define W25_DEVICE_SIZE         (16*1024*1024) // bytes (128MBit = 16MBytes)
#define W25_SECTOR_COUNT        (W25_DEVICE_SIZE/W25_SECTOR_SIZE)

/* W25Q128 chip select is active LOW */
#define W25_CS_ENABLE()   nCS_PA18_Clear()
#define W25_CS_DISABLE()  nCS_PA18_Set()

// Return 3 byte Manufacturer and device ID (requires 3 byte buffer)
int W25_ReadJedecID(uint8_t *buf, int bufSize);
// Return 8 byte Unique ID (requires 8 byte buffer)
int W25_ReadUniqueID(uint8_t *buf, int bufSize);
// Return Status1 register
uint8_t W25_ReadStatusReg1(void);
// Returns 0:Not busy, or 1:Busy
int W25_Busy(void);
int W25_DelayWhileBusy(uint32_t msTimeout); // Delay up to TIMEOUT value
// Send Write Enable command, allowing writing to the device
int W25_WriteEnable(void);
// Send Write Disable command, preventing writing to the device
int W25_WriteDisable(void);
// Read Data from the FLASH device - no limit (the whole device can be read with this)
int W25_ReadData(uint32_t address, uint8_t *buf, int bufSize);
// Write data (updated to cross page boundaries)
int W25_PageProgram(uint32_t address, uint8_t *buf, uint32_t count);
// Erase a 4096 byte sector
int W25_SectorErase(uint32_t address);
// Erase the entire chip (May take 40 seconds or more depending on the device)
int W25_ChipErase(void);

//=================================================================================================
// Command Line Interface functions:
//=================================================================================================
// Read and display Jedec Device ID
int cl_w25_id(void);

// Read and display 8 byte unique ID
int cl_w25_unique_id(void);

// Write enable
int cl_w25_write_enable(void);

// Write disable
int cl_w25_write_disable(void);

// Read flash and display with hexdump
int cl_w25_read(void);

// Erase sector <Address>
int cl_w25_erase(void);

// Command line COMMAND_ITEM data (see command_line.c):
#define FLASH_TEST_COMMANDS \
{"w25id",    "Read Jedec Device ID from w25q128",     cl_w25_id}, \
{"w25uid",   "Read Unique ID from w25q128",           cl_w25_unique_id}, \
{"w25wen",   "Write Enable",                          cl_w25_write_enable}, \
{"w25dis",   "Write Disable",                         cl_w25_write_disable}, \
{"w25read",  "Read <Address> <Length>",               cl_w25_read}, \
{"w25erase", "Erase sector <Address>",                cl_w25_erase} \

#endif // _w25q128_h_
