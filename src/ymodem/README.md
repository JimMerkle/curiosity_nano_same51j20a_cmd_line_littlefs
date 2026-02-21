# Y-Modem Protocol Implementation

## Overview

This module provides a complete Y-Modem (CRC) implementation for embedded systems, specifically designed for the SAMD51 microcontroller with LittleFS filesystem support. The implementation supports both batch-capable file reception and single/multi-file transmission.

## Features

- **Batch Reception**: Receive multiple files in a single session
- **Batch Transmission**: Send multiple files sequentially  
- **CRC Error Detection**: Uses 16-bit CRC for reliable data transfer
- **Variable Block Sizes**: Supports both 128-byte (SOH) and 1024-byte (STX) packets
- **Timeout Handling**: Configurable timeouts for robust communication
- **LittleFS Integration**: Direct file system operations
- **Debug Support**: Conditional debug output via SERCOM2

## Hardware Requirements

- **SAMD51 Microcontroller** (or compatible SAME5x series)
- **SERCOM5**: Data communication channel with ring buffer support
  - TX Buffer: 4096 bytes (shared with logger functionality)  
  - RX Buffer: 2100 bytes
- **SERCOM2**: Debug output channel (optional, enabled with `YMODEM_DEBUG`)
- **SysTick Timer**: For timing and delays

## Protocol Specifications

### Y-Modem Protocol Details
- **Block 0**: Metadata block containing filename and file size
- **Data Blocks**: Sequential numbered blocks (1, 2, 3, ...)
- **End of Transmission**: EOT character followed by ACK handshake
- **Batch Termination**: Empty Block 0 to end session

### Packet Structure
```
SOH/STX | Block# | ~Block# | Data[128/1024] | CRC16_H | CRC16_L
   1       1        1         128/1024         1         1
```

### Control Characters
- `SOH (0x01)`: Start of 128-byte block
- `STX (0x02)`: Start of 1024-byte block  
- `EOT (0x04)`: End of transmission
- `ACK (0x06)`: Acknowledge
- `NAK (0x15)`: Negative acknowledge
- `CAN (0x18)`: Cancel transmission
- `'C' (0x43)`: CRC request

## API Reference

### Core Functions

#### `int ymodem_receive(void)`
Initiates batch-capable Y-Modem reception.

**Returns (ymodem_result_t):**
- `YMODEM_SUCCESS (0)`: All files received successfully
- `YMODEM_ERR_GENERAL (-1)`: File system or protocol error
- `YMODEM_ERR_CANCELLED (-2)`: Transfer cancelled by sender
- `YMODEM_ERR_FILE_EXISTS (-3)`: File already exists (use 'remove' command first)

**Behavior:**
- Sends periodic 'C' characters to invite transmission
- Creates new LittleFS files from Block 0 metadata
- Handles multiple files in sequence
- Terminates on empty Block 0

#### `int ymodem_transmit(const char *path)`
Transmits a single file via Y-Modem protocol.

**Parameters:**
- `path`: LittleFS path to file for transmission

**Returns (ymodem_result_t):**
- `YMODEM_SUCCESS (0)`: File transmitted successfully
- `YMODEM_ERR_GENERAL (-1)`: File system or protocol error
- `YMODEM_ERR_CANCELLED (-2)`: Transfer cancelled by receiver

**Behavior:**
- Waits for receiver's 'C' request
- Sends Block 0 with filename and size
- Transmits file data in optimal block sizes
- Handles retransmission on NAK

#### `int cl_ymodem(void)`
Command-line interface for Y-Modem operations.

**Usage:**
- No arguments: Receive mode
- One or more arguments: Transmit mode (batch)

**Returns (ymodem_result_t):**
- Same codes as `ymodem_receive()` and `ymodem_transmit()`
- Displays user-friendly error messages for file exists condition

## Configuration Constants

```c
#define YM_BLOCK_SIZE_128  128     // Small block size
#define YM_BLOCK_SIZE_1K   1024    // Large block size  
#define YM_MAX_RETRY       10      // Maximum retry attempts
#define YM_IO_TIMEOUT_MS   3000    // I/O operation timeout
#define YM_INITIAL_C_MS    3000    // Initial 'C' request interval
#define YM_INTERPACKET_MS  10      // Inter-packet delay
```

## Usage Examples

### Receiving Files
```c
// Start Y-Modem reception (waits for sender)
int result = ymodem_receive();
if (result == YMODEM_SUCCESS) {
    printf("All files received successfully\n");
} else if (result == YMODEM_ERR_FILE_EXISTS) {
    printf("File already exists - delete it first\n");
} else if (result == YMODEM_ERR_CANCELLED) {
    printf("Transfer cancelled\n");
} else {
    printf("Reception error: %d\n", result);
}
```

### Transmitting a Single File  
```c
// Send a single file
int result = ymodem_transmit("/config/settings.ini");
if (result == YMODEM_SUCCESS) {
    printf("File sent successfully\n");
}
```

### Command Line Usage
```c
// Setup command line arguments
argc = 2;
argv[0] = "ymodem";
argv[1] = "/data/logfile.txt";

// Execute Y-Modem command
int result = cl_ymodem();
```

## Error Handling

### Return Codes (ymodem_result_t enum)
All functions return standardized codes defined in `ymodem.h`:
- `YMODEM_SUCCESS (0)`: Operation completed successfully
- `YMODEM_ERR_GENERAL (-1)`: File system, timeout, CRC, or protocol error
- `YMODEM_ERR_CANCELLED (-2)`: Transfer cancelled via CAN characters
- `YMODEM_ERR_FILE_EXISTS (-3)`: Receive failed - file already exists

### Common Error Scenarios
- **Timeout Errors**: No response within configured timeouts
- **CRC Errors**: Data corruption detected during transmission  
- **File Already Exists**: User must delete existing file first using 'remove' command
- **Protocol Errors**: Invalid block numbers or packet format
- **Cancellation**: Either party can cancel with CAN characters

### Retry Mechanism
- Automatic retransmission on NAK or timeout
- Maximum retry limit prevents infinite loops
- Exponential backoff for robust recovery

## Integration Requirements

### Dependencies
The module requires these external components:

```c
// System timing
uint32_t SYSTICK_GetTickCounter(void);
void SYSTICK_DelayMs(uint32_t ms);

// UART communication (SERCOM5)
size_t SERCOM5_USART_Write(uint8_t* buffer, size_t size);
size_t SERCOM5_USART_Read(uint8_t* buffer, size_t size);
size_t SERCOM5_USART_ReadCountGet(void);
size_t SERCOM5_USART_WriteFreeBufferCountGet(void);

// Debug output (SERCOM2, optional)
size_t SERCOM2_USART_Write(uint8_t* buffer, size_t size);

// LittleFS file system
extern lfs_t lfs;
int lfs_file_open(lfs_t *lfs, lfs_file_t *file, const char *path, int flags);
int lfs_file_close(lfs_t *lfs, lfs_file_t *file);
lfs_ssize_t lfs_file_read(lfs_t *lfs, lfs_file_t *file, void *buffer, lfs_size_t size);
lfs_ssize_t lfs_file_write(lfs_t *lfs, lfs_file_t *file, const void *buffer, lfs_size_t size);
lfs_soff_t lfs_file_seek(lfs_t *lfs, lfs_file_t *file, lfs_soff_t off, int whence);
```

### Build Configuration
Enable debug output by defining:
```c
#define YMODEM_DEBUG
```

## Performance Characteristics

### Transfer Rates
- **128-byte blocks**: ~9.6 KB/s at 115200 baud
- **1024-byte blocks**: ~10.4 KB/s at 115200 baud (optimal)
- **Efficiency**: ~90% protocol efficiency with 1K blocks

### Memory Usage
- **Stack**: ~1.2KB during operation (context structure)
- **Flash**: ~6KB code size
- **RAM Buffers**: Uses existing SERCOM ring buffers

### Timing Requirements
- **Baud Rate**: Tested at 115200 and 230400 bps
- **Flow Control**: none
- **Latency**: <10ms typical response time

## Troubleshooting

### Common Issues

1. **Transfer Hangs**
   - Check UART configuration and baud rate
   - Verify cable connections
   - Enable flow control if needed

2. **CRC Errors**
   - Check for electrical noise
   - Verify ground connections
   - Reduce baud rate if persistent

3. **File Creation Fails**
   - Ensure LittleFS is properly mounted
   - Check available flash space
   - Verify file path format

4. **Timeout Issues**
   - Increase timeout values for slow systems
   - Check SysTick timer configuration
   - Verify interrupt priorities

### Debug Output
When `YMODEM_DEBUG` is enabled, detailed protocol information is output via SERCOM2:

```
[YMRX] start
[YMRX] file done: config.ini (1024 bytes)
[YMRX] end batch
[YMTX] settings.dat size=2048
[YMTX] done
```

## Version History

- **v1.1** (2026-02-05): Enhanced error handling and API improvements
  - Added `ymodem_result_t` enum with named error codes
  - Added `YMODEM_ERR_FILE_EXISTS` for explicit file conflict detection
  - Improved user feedback in `cl_ymodem()` for common errors
  - Updated public API declarations in ymodem.h
  - Fixed single file receive timeout - now returns to CLI in 500ms instead of 30+ seconds
  - Smart batch detection: distinguishes single file from batch transfers automatically
  
- **v1.0** (2025-11-21): Initial implementation
  - Batch reception support
  - Single file transmission
  - CRC error detection
  - LittleFS integration
  - Command line interface

## License

## Author

**Jim Merkle**  
Date: November 21, 2025