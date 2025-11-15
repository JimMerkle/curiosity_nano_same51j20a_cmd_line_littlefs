// Copyright Jim Merkle, 2/17/2020
// Module: w25q128.c
// Updated to work with MPLABX / Harmony SERCOM SPI APIs

//#include <stdio.h> // printf()
#include <string.h>
#include <stdlib.h> // strtol()
#include <stdbool.h>
#include "littlefs/w25q128.h"
#include "littlefs/lfs.h"
#include "definitions.h"                // SYS function prototypes
#include "command_line/command_line.h"
#include "logger/logger.h"          // use log_msg())

#define ADD_CHECKS 1

void hexDump(void * address, int bytes); // external hexDump routine

// Return 3 byte JEDEC Manufacturer and device ID (requires 3 byte buffer)
// Winbond 8.2.29 Read JEDEC ID (9Fh)
int W25_ReadJedecID(uint8_t *buf, int bufSize) {
  uint8_t idcmd = W25_CMD_READ_JEDEC_ID;
  if(bufSize < W25_JEDEC_ID_BUF_SIZE)
    return LFS_ERR_INVAL; // buffer too small - invalid parameter
  W25_CS_ENABLE(); // Drive Winbond chip select, /CS low
  //nCS_PA18_Clear();
  bool rc = SERCOM1_SPI_Write(&idcmd , sizeof(idcmd));
  if(rc) {
    rc = SERCOM1_SPI_Read(buf,bufSize);
  }
  W25_CS_DISABLE();
  //log_msg("%s: rc %d, %02X, %02X, %02X\r\n",__func__, rc, buf[0],buf[1],buf[2]);
  return rc?LFS_ERR_OK:LFS_ERR_IO;
} // W25_ReadJEDECID()

// Return 8 byte Unique ID Number (requires 8 byte return buffer)
// Winbond 8.2.28 Read Unique ID Number (4bh)
int W25_ReadUniqueID(uint8_t *buf, int bufSize) {
  uint8_t cmddata[5] = {W25_CMD_READ_UNIQUE_ID,0,0,0,0};
  if(bufSize < W25_UNIQUE_ID_BUF_SIZE)
    return LFS_ERR_INVAL; // buffer too small - invalid parameter
  W25_CS_ENABLE(); // Drive Winbond chip select, /CS low
  bool rc = SERCOM1_SPI_Write(cmddata , sizeof(cmddata ));
  if(rc) {
    rc = SERCOM1_SPI_Read(buf,bufSize);
  }
  W25_CS_DISABLE();
  //log_msg("%s: rc %d, ",__func__, retval);
  //hexDump(buf,bufSize);
  //log_msg("\r\n");
  return rc?LFS_ERR_OK:LFS_ERR_IO;
} // W25_ReadUniqueID()

// Returns value of Status Register-1 (byte)
// Winbond 8.2.4 Read Status Register-1 (05h)
// See section 7.1 for bit values
uint8_t W25_ReadStatusReg1(void) {
  uint8_t cmd = W25_CMD_READ_STATUS_REG_1;
  uint8_t status_reg1;
  W25_CS_ENABLE(); // Drive Winbond chip select, /CS low
  int rc = SERCOM1_SPI_Write(&cmd , sizeof(cmd));
  if(rc) {
    rc = SERCOM1_SPI_Read(&status_reg1, sizeof(status_reg1));
  }
  W25_CS_DISABLE();
  //log_msg("%s: 0x%02X, ",__func__, status_reg1);
  return rc ? status_reg1:0xFF; // return 0xFF if error
} // W25_ReadStatusReg1()

// Send Write Enable command
// Winbond 8.2.1 Write Enable (06h)
// See section 7.1, page 17, and section 8.2.1, page 30
// This sets the WEL bit, S1, in status register 1, allowing the part to be written.
int W25_WriteEnable(void) {
  uint8_t cmd = W25_CMD_WRITE_ENABLE;
  //log_msg("+%s()\r\n",__func__);
  W25_CS_ENABLE(); // Drive Winbond chip select, /CS low
  int rc = SERCOM1_SPI_Write(&cmd , sizeof(cmd));
  W25_CS_DISABLE();
  return rc?LFS_ERR_OK:LFS_ERR_IO;
} // W25_WriteEnable()

// Send Write Disable command
// Winbond 8.2.1 Write Disable (04h)
// See section 7.1, page 17, and section 8.2.1, page 30
// This clears the WEL bit, S1, in status register 1, preventing writing to the part
int W25_WriteDisable(void) {
  uint8_t cmd = W25_CMD_WRITE_DISABLE;
  W25_CS_ENABLE(); // Drive Winbond chip select, /CS low
  int rc = SERCOM1_SPI_Write(&cmd , sizeof(cmd));
  W25_CS_DISABLE();
  return rc?LFS_ERR_OK:LFS_ERR_IO;
} // W25_WriteDisable()

// Winbond 8.2.6 Read Data (03h)
// The only limit for quantity of data is memory / device size
int W25_ReadData(uint32_t address, uint8_t *buf, int bufSize)
{
  uint8_t cmdaddr[4] = {W25_CMD_READ_DATA,address>>16,address>>8,address};
  //log_msg("+%s(Addr 0x%06X, buf 0x%08X, Len 0x%04X)\r\n",__func__,address,buf,bufSize);
  W25_CS_ENABLE(); // Drive Winbond chip select, /CS low
  int rc = SERCOM1_SPI_Write(cmdaddr , sizeof(cmdaddr));
  if(!rc) {
    log_msg("%s: HAL_SPI_Transmit() error %d\r\n",__func__,rc);
    return rc;
  }
  //memset(buf,0,bufSize); // Buffer is transmitted during receive   
  rc = SERCOM1_SPI_Read(buf, bufSize);
  if(!rc)
    log_msg("%s: HAL_SPI_Receive() error %d\r\n",__func__, rc);

  W25_CS_DISABLE();
  //hexDump(buf,bufSize);
  return rc?LFS_ERR_OK:LFS_ERR_IO;
} // W25_ReadData()

// Winbond 8.2.15 Page Program (02h)
// Write one byte up to 256 bytes (a page) of data (original comment - now supports multiple page writes)
// This function has been upgraded to write multiple pages, using multiple page write commands.
// LittleFS is unaware of the page boundary issue.  Manage the issue with multiple writes
int W25_PageProgram(uint32_t address, uint8_t *buf, uint32_t count)
{
  int rc;
  //log_msg("+%s(Addr 0x%06X, Len 0x%04X)\r\n",__func__,address,count);
  W25_WriteEnable(); // Make sure we can write...
  while(count) {
    uint8_t cmdaddr[4] = {W25_CMD_PAGE_PROGRAM,address>>16,address>>8,address};
    uint32_t space_left_in_page = 0x100 - (address & 0xFF);
    uint32_t count_this_pass = count <= space_left_in_page? count:space_left_in_page;
    W25_CS_ENABLE(); // Drive Winbond chip select, /CS low
    rc = SERCOM1_SPI_Write(cmdaddr , sizeof(cmdaddr)); // Send Page Program command with address
    if(!rc) {
      // Failure!
    	W25_CS_DISABLE();
    	log_msg("%s: HAL_SPI_Transmit() error %d\r\n",__func__,rc);
    	return LFS_ERR_IO;
    }
    rc = SERCOM1_SPI_Write(buf, count_this_pass); // Write page data
    if(!rc) {
    	// Failure!
      W25_CS_DISABLE();
    	log_msg("%s: HAL_SPI_Transmit() error %d\r\n",__func__,rc);
    	return LFS_ERR_IO;
    }
    W25_CS_DISABLE();
    count -= count_this_pass;
    address += count_this_pass;
    buf += count_this_pass;
    rc = W25_DelayWhileBusy(PAGE_PROGRAM_TIMEOUT);
    if(rc) {
      // If non-zero, "busy" after timeout
    	W25_CS_DISABLE();
    	log_msg("%s: W25_DelayWhileBusy() error %d\r\n",__func__,rc);
    	return LFS_ERR_IO;
    }
  } // while(count)
  rc = LFS_ERR_OK;
  //log_msg("%s: retval %d, ",__func__, retval);
  return rc;
} // W25_PageProgram()

// Winbond 8.2.17 Sector Erase (20h)
// Erase all data within the addressed 4K sector.
int W25_SectorErase(uint32_t address)
{
  uint8_t cmdaddr[4] = {W25_CMD_SECTOR_ERASE,address>>16,address>>8,address};
  W25_WriteEnable(); // Make sure we can write...
  //log_msg("+%s(Addr 0x%06X)\r\n",__func__,address);
  W25_CS_ENABLE(); // Drive Winbond chip select, /CS low
  bool rc = SERCOM1_SPI_Write(cmdaddr , sizeof(cmdaddr )); // Send Sector Erase command with address
  W25_CS_DISABLE();
  //log_msg("%s: retval %d, ",__func__, retval);
  W25_DelayWhileBusy(SECTOR_ERASE_TIMEOUT);
  return rc?LFS_ERR_OK:LFS_ERR_IO;
} // W25_SectorErase()

// Winbond 8.2.20 Chip Erase (60h)
// Erase all data within the FLASH device
int W25_ChipErase(void)
{
  uint8_t cmd = {W25_CMD_CHIP_ERASE};
  //log_msg("+%s()\r\n",__func__);
  W25_WriteEnable(); // Make sure we can write...
  W25_CS_ENABLE(); // Drive Winbond chip select, /CS low
  bool rc = SERCOM1_SPI_Write(&cmd , sizeof(cmd )); // Send Chip Erase command
  W25_CS_DISABLE();
  //log_msg("%s: retval %d, ",__func__, retval);
  W25_DelayWhileBusy(CHIP_ERASE_TIMEOUT);
  return rc?LFS_ERR_OK:LFS_ERR_IO;
} // W25_ChipErase()

// Returns 0:Not busy, or 1:Busy
int W25_Busy(void)
{
  return (W25_ReadStatusReg1() & W25_STATUS1_BUSY);
}

// Loop while busy and not timeout
int W25_DelayWhileBusy(uint32_t msTimeout)
{
  uint32_t initial_count = SYSTICK_GetTickCounter();
  int busy;
  uint32_t deltaticks;
  uint32_t count = 0;
  do {
    busy = W25_Busy();
    deltaticks = SYSTICK_GetTickCounter() - initial_count;
    count++;
  } while(busy && deltaticks < msTimeout);
  //log_msg("%s() time(ms): %u, busy: %u\r\n",__func__,deltaticks,busy);
  return busy;
}

//=================================================================================================
// Command Line Interface functions:
// These are for testing the SPI memory library.
//=================================================================================================
// Read and display Jedec Device ID
int cl_w25_id(void)
{
	uint8_t buf[3];
	int rc = W25_ReadJedecID(buf, sizeof(buf));
	log_msg("Jedec ID: 0x%02X 0x%02X 0x%02X\n",buf[0],buf[1],buf[2]);
	return rc;
}

// Read and display 8 byte unique ID
int cl_w25_unique_id(void)
{
	uint8_t buf[8];
	int rc = W25_ReadUniqueID(buf, sizeof(buf));
	log_msg("Unique ID: ");
	for(int i=0;i<8;i++) log_msg("%02X ",buf[i]);
	log_msg("\n");
	return rc;
}

// Write enable
int cl_w25_write_enable(void)
{
	int rc = W25_WriteEnable();
	log_msg("Write Enable: rc: %d\n",rc);
	return rc;
}

// Write disable
int cl_w25_write_disable(void)
{
	int rc = W25_WriteDisable();
	log_msg("Write Disable: rc: %d\n",rc);
	return rc;
}

#define MAX_CL_READ 0x4000 // 16K
#define MAX_CL_MAX_ADDRESS 0x1000000 // 16MB
void hexdump(void * address, uint32_t count, uint32_t address_value); // hexdump.c

// Read flash and display with hexdump
int cl_w25_read(void)
{
    // verify argument count, to remove it from the table
    if(argc < 3) {
       log_msg("\r\nInvalid Arg cnt: %d Expected: %d\n", argc - 1, 2);
       return 0;
   }
	uint8_t buf[256];
	uint32_t address = strtol(argv[1],NULL,0); // allow both decimal and hex
	uint32_t length = strtol(argv[2],NULL,0); // allow both decimal and hex
	log_msg("%s: Address 0x%06lX, Length: 0x%lX\n\n",__func__, address, length);
	if(length > MAX_CL_READ) length = MAX_CL_READ;
	if(address + length >= MAX_CL_MAX_ADDRESS) {
		log_msg("%s: ERROR!  Address + length too large!\n",__func__);
		return 1;
	}
	uint32_t remaining = length;

	do {
		// Determine how many bytes to read & display
		uint32_t this_pass = remaining < sizeof(buf)? remaining:sizeof(buf);

		// Read the data from the flash
		int rc = W25_ReadData(address, buf, this_pass);
	    if(rc != LFS_ERR_OK) {
	    	log_msg("%s W25_ReadData() ERROR!\n",__func__);
	    	return 2;
	    }
		hexdump(buf, this_pass, address); // hexdump.c
		remaining -= this_pass;
		address += this_pass;
      SYSTICK_DelayMs(115);  // 256 bytes = 16 lines * 80 characters / line * 10 USART bits / character / 115200 = 111.1 ms
	} while(remaining > 0);
	return 0;
}

// Erase sector <Address>
int cl_w25_erase(void)
{    
    // verify argument count, to remove it from the table
    if(argc < 2) {
       log_msg("\r\nInvalid Arg cnt: %d Expected: %d\n", argc - 1, 1);
       return 0;
   }
	uint32_t address = strtol(argv[1],NULL,0); // allow both decimal and hex
	// Mask the address to the start of a sector
	address &= ~(W25_SECTOR_SIZE-1);
	log_msg("%s: Address 0x%06lX\n\n",__func__, address);
	if(address >= MAX_CL_MAX_ADDRESS) {
		log_msg("%s: ERROR!  Address too large!\n",__func__);
		return 1;
	}
	// Erase the requested flash sector
	int rc = W25_SectorErase(address);
	if(rc != LFS_ERR_OK) {
		log_msg("%s W25_SectorErase() ERROR, %d\n",__func__, rc);
		return 2;
	}

	return 0;
}

// Chip erase
int cl_w25_chip_erase(void)
{
	log_msg("%s: Erasing chip.  May take 40 seconds or more...\n",__func__);
	// Erase the requested flash sector
	int rc = W25_ChipErase();
	if(rc != LFS_ERR_OK) {
		log_msg("%s W25_ChipErase() ERROR, %d\n",__func__, rc);
		return 2;
	}
	return 0;
}
