// ymodem.c
// Y-Modem (CRC) transmit/receive for SAME51J20A
// Data: SERCOM5; Debug: SERCOM2 via dbg_msg()
// Filesystem: LittleFS
//
// This module implements batch-capable receive and single-file transmit.
// Empty block 0 ends a batch session.
// - Receive: ymodem_receive()
// - Transmit: ymodem_transmit(const char *path)
//
// Requirements (provided elsewhere):
//   - SYSTICK_DelayMs(uint32_t ms)
//   - SYSTICK_GetTickCounter(void)
//   - dbg_msg(const char *fmt, ...)
//   - LittleFS globals and functions
//   - SERCOM5 USART functions
//
// Author: Jim Merkle
// Date: 11/20/2025

#include "logger/logger.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

// SAMD / SAME definitions
#include "definitions.h"                // SYS function prototypes
#include "sam.h"
#include "littlefs/lfs.h"
#include "littlefs/lfs_interface.h"
#include "command_line/command_line.h"
#include "ymodem/ymodem.h"

#define YMODEM_DEBUG  1

#if YMODEM_DEBUG
#define PRINTF_BUF_SIZE             128

// Debug message using SERCOM2 - FTDI module
// Used exclusivly for debugging ymodem module
int dbg_msg(const char *fmt, ...) {
    char print_buf[PRINTF_BUF_SIZE];
    // Format user message
    va_list args;
    va_start(args, fmt);
    int msg_len = vsnprintf(print_buf, PRINTF_BUF_SIZE, fmt, args);
    va_end(args);
    // If message doesn't fit, throw it out
    //if (msg_len < 0 || msg_len > PRINTF_BUF_SIZE) return 0;

    if (msg_len < 0) return 0; // Error creating message
    // If message is too long, but we have it in the buffer, use it
    if (msg_len >= PRINTF_BUF_SIZE) msg_len = PRINTF_BUF_SIZE-1; // always a null on the end
    
    // How much space is available in TX ring buffer?
    uint32_t tx_space_available = SERCOM2_USART_WriteFreeBufferCountGet();
    if(tx_space_available >= (uint32_t)msg_len) {
        // Enough space in ring buffer...
        SERCOM2_USART_Write((uint8_t *)print_buf, msg_len);
        return msg_len;
    }
    dropped_messages++;
    return 0;
} // dbg_msg())
#else
#define int dbg_msg(fmt, ...) /* define out of existance */
#endif // YMODEM_DEBUG

//// ---- External hooks you must provide in your project ----
//
//// Timing
//extern void SYSTICK_DelayMs(uint32_t delay_ms);
//extern uint32_t SYSTICK_GetTickCounter(void);
//
//// Debug (SERCOM2)
//extern int dbg_msg(const char *fmt, ...);
//
//// SERCOM5 data I/O
//extern size_t SERCOM5_USART_Write(uint8_t* pWrBuffer, const size_t size);
//extern size_t SERCOM5_USART_Read(uint8_t* pRdBuffer, const size_t size);
//extern size_t SERCOM5_USART_ReadCountGet(void);
//
//// LittleFS
//typedef struct lfs lfs_t;
//typedef struct lfs_file lfs_file_t;
//extern lfs_t lfs;
//extern int lfs_file_open(lfs_t *lfs, lfs_file_t *file, const char *path, int flags);
//extern int lfs_file_write(lfs_t *lfs, lfs_file_t *file, const void *buffer, size_t size);
//extern int lfs_file_read(lfs_t *lfs, lfs_file_t *file, void *buffer, size_t size);
//extern int lfs_file_close(lfs_t *lfs, lfs_file_t *file);
//extern int lfs_remove(lfs_t *lfs, const char *path);
//extern long lfs_file_seek(lfs_t *lfs, lfs_file_t *file, long off, int whence);
//
//// LittleFS flags (include correct header in your project)
//#ifndef LFS_O_RDONLY
//#define LFS_O_RDONLY 0x1
//#endif
//#ifndef LFS_O_WRONLY
//#define LFS_O_WRONLY 0x2
//#endif
//#ifndef LFS_O_CREAT
//#define LFS_O_CREAT  0x0100
//#endif
//#ifndef LFS_O_EXCL
//#define LFS_O_EXCL   0x0200
//#endif
//#ifndef LFS_SEEK_SET
//#define LFS_SEEK_SET 0
//#endif

// ---- Protocol constants ----
#define SOH     0x01
#define STX     0x02
#define EOT     0x04
#define ACK     0x06
#define NAK     0x15
#define CAN     0x18
#define CRC_REQ 'C'

#define YM_BLOCK_SIZE_128  128
#define YM_BLOCK_SIZE_1K   1024
#define YM_MAX_RETRY       10
#define YM_IO_TIMEOUT_MS   3000
#define YM_INITIAL_C_MS    3000
#define YM_INTERPACKET_MS  10

// ---- Internal helpers ----
static inline int ymodem_putc(uint8_t c) {
    return (SERCOM5_USART_Write(&c, 1) == 1) ? 0 : -1;
}

static inline int ymodem_write(const uint8_t *buf, size_t len) {
    return (SERCOM5_USART_Write((uint8_t*)buf, len) == len) ? 0 : -1;
}

// Read len bytes with timeout. Returns bytes read (may be < len if timeout).
static int ymodem_read(uint8_t *buf, size_t len, uint32_t timeout_ms) {
    uint32_t start = SYSTICK_GetTickCounter();
    size_t got = 0;
    while (got < len) {
        size_t avail = SERCOM5_USART_ReadCountGet();
        if (avail > 0) {
            size_t n = SERCOM5_USART_Read(buf + got, len - got);
            got += n;
            if (got == len) break;
        } else {
            if ((SYSTICK_GetTickCounter() - start) >= timeout_ms) break;
            SYSTICK_DelayMs(1);
        }
    }
    return (int)got;
}

static inline int ymodem_getc(uint8_t *c, uint32_t timeout_ms) {
    return (ymodem_read(c, 1, timeout_ms) == 1) ? 0 : -1;
}

static uint16_t ymodem_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (uint16_t)((crc << 1) ^ 0x1021);
            else              crc <<= 1;
        }
    }
    return crc;
}

// ---- Context ----
typedef struct {
    uint8_t payload[YM_BLOCK_SIZE_1K];
    uint8_t blk_num;
    uint32_t file_size;
    uint32_t bytes_done;
    char filename[128];
    lfs_file_t file;
} ymodem_ctx_t;

static int ymodem_ack(void) { return ymodem_putc(ACK); }
static int ymodem_nak(void) { return ymodem_putc(NAK); }
static int ymodem_send_crc_req(void) { return ymodem_putc(CRC_REQ); }
static void ymodem_can2(void) { (void)ymodem_putc(CAN); (void)ymodem_putc(CAN); }

// Build and send a Y-modem packet (SOH/STX, blk, ~blk, payload, CRC)
static int ymodem_send_packet(uint8_t type, uint8_t blk, const uint8_t *data, size_t len) {
    uint8_t hdr[3] = { type, blk, (uint8_t)(~blk) };
    if (ymodem_write(hdr, 3) != 0) return -1;

    size_t payload_len = (type == SOH) ? YM_BLOCK_SIZE_128 : YM_BLOCK_SIZE_1K;
    uint8_t tmp[YM_BLOCK_SIZE_1K];
    memset(tmp, 0x1A, payload_len); // CP/M EOF padding
    if (len > payload_len) len = payload_len;
    if (data && len) memcpy(tmp, data, len);

    if (ymodem_write(tmp, payload_len) != 0) return -2;

    uint16_t crc = ymodem_crc16(tmp, payload_len);
    uint8_t crc_bytes[2] = { (uint8_t)(crc >> 8), (uint8_t)(crc & 0xFF) };
    if (ymodem_write(crc_bytes, 2) != 0) return -3;

    return 0;
}

// Read one packet: returns 0 on success, sets ptype, blk, plen; handles EOT/CAN
static int ymodem_read_packet(ymodem_ctx_t *ctx, uint8_t *ptype, uint8_t *blk, size_t *plen) {
    uint8_t c;
    if (ymodem_getc(&c, YM_IO_TIMEOUT_MS) != 0) return -1;

    if (c == EOT || c == CAN) {
        *ptype = c;
        return 0;
    }

    if (c != SOH && c != STX) return -2;
    *ptype = c;

    size_t payload_len = (c == SOH) ? YM_BLOCK_SIZE_128 : YM_BLOCK_SIZE_1K;

    uint8_t hdr[2];
    if (ymodem_read(hdr, 2, YM_IO_TIMEOUT_MS) != 2) return -3;
    *blk = hdr[0];
    uint8_t blk_inv = hdr[1];
    if ((uint8_t)(~(*blk)) != blk_inv) return -4;

    if (ymodem_read(ctx->payload, payload_len, YM_IO_TIMEOUT_MS) != (int)payload_len) return -5;

    uint8_t crc_bytes[2];
    if (ymodem_read(crc_bytes, 2, YM_IO_TIMEOUT_MS) != 2) return -6;
    uint16_t recv_crc = ((uint16_t)crc_bytes[0] << 8) | crc_bytes[1];
    uint16_t calc_crc = ymodem_crc16(ctx->payload, payload_len);
    if (recv_crc != calc_crc) return -7;

    *plen = payload_len;
    return 0;
}

// Parse block 0: filename\0size\0... Returns 0 ok, 1 empty (end batch), <0 error
static int parse_block0(ymodem_ctx_t *ctx, size_t len) {
    const char *p = (const char *)ctx->payload;
    if (len == 0 || *p == '\0') return 1; // empty => end batch

    size_t fn_len = strnlen(p, len);
    if (fn_len == 0 || fn_len >= sizeof(ctx->filename)) return -1;

    memcpy(ctx->filename, p, fn_len);
    ctx->filename[fn_len] = '\0';

    // size field
    if (fn_len + 1 >= len) return -2;
    p += fn_len + 1;
    size_t sz_len = strnlen(p, len - (fn_len + 1));
    if (sz_len == 0) return -3;

    ctx->file_size = (uint32_t)strtoul(p, NULL, 10);
    return 0;
}

// ---- Public API ----

// Batch-capable receive. Creates new LittleFS files from block 0.
// Returns 0 on success, -1 on error, -2 if cancelled.
int ymodem_receive(void) {
    ymodem_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    dbg_msg("[YMRX] start\n");

    // Initial 'C' to invite sender (and periodically while idle)
    uint32_t last_c = 0;

    while (1) {
        // Issue periodic 'C' while waiting for block 0
        uint32_t now = SYSTICK_GetTickCounter();
        if ((now - last_c) > YM_INITIAL_C_MS) {
            ymodem_send_crc_req();
            last_c = now;
        }

        uint8_t type = 0, blk = 0;
        size_t len = 0;
        int r = ymodem_read_packet(&ctx, &type, &blk, &len);
        if (r == 0 && (type == SOH || type == STX) && blk == 0) {
            // Metadata block
            int pr = parse_block0(&ctx, len);
            if (pr == 1) {
                // End of batch
                ymodem_ack();
                dbg_msg("[YMRX] end batch\n");
                return 0;
            }
            if (pr < 0) {
                dbg_msg("[YMRX] bad block0 (%d)\n", pr);
                ymodem_nak();
                continue;
            }

            // Open file for write (exclusive create)
            if (lfs_file_open(&lfs, &ctx.file, ctx.filename,
                              LFS_O_WRONLY | LFS_O_CREAT | LFS_O_EXCL) < 0) {
                dbg_msg("[YMRX] open fail: %s\n", ctx.filename);
                ymodem_can2();
                return -1;
            }

            ctx.bytes_done = 0;
            ctx.blk_num = 1;

            ymodem_ack();
            ymodem_send_crc_req(); // prompt first data block

            // Receive data blocks for this file
            while (1) {
                r = ymodem_read_packet(&ctx, &type, &blk, &len);
                if (r == 0 && (type == SOH || type == STX)) {
                    if (blk == ctx.blk_num) {
                        // Trim to claimed size
                        size_t to_write = len;
                        if (ctx.file_size > 0) {
                            uint32_t remaining = ctx.file_size - ctx.bytes_done;
                            if (remaining < to_write) to_write = remaining;
                        }

                        if (to_write > 0) {
                            if (lfs_file_write(&lfs, &ctx.file, ctx.payload, to_write) < 0) {
                                dbg_msg("[YMRX] write error\n");
                                ymodem_can2();
                                lfs_file_close(&lfs, &ctx.file);
                                return -1;
                            }
                        }
                        ctx.bytes_done += (uint32_t)to_write;

                        ymodem_ack();
                        ctx.blk_num++;
                    } else if (blk == (uint8_t)(ctx.blk_num - 1)) {
                        // Duplicate block (sender didn't see our ACK). Re-ACK.
                        ymodem_ack();
                    } else {
                        dbg_msg("[YMRX] unexpected blk %u (expect %u)\n", blk, ctx.blk_num);
                        ymodem_nak();
                    }
                } else if (r == 0 && type == EOT) {
                    // End-of-file
                    ymodem_ack();
                    lfs_file_close(&lfs, &ctx.file);
                    dbg_msg("[YMRX] file done: %s (%lu bytes)\n", ctx.filename, (unsigned long)ctx.bytes_done);

                    // Invite next file (block 0)
                    ymodem_send_crc_req();
                    break; // go back to outer loop for next block 0
                } else if (r == 0 && type == CAN) {
                    dbg_msg("[YMRX] cancelled by sender\n");
                    lfs_file_close(&lfs, &ctx.file);
                    return -2;
                } else {
                    dbg_msg("[YMRX] pkt read error (%d), NAK\n", r);
                    ymodem_nak();
                }
            }

            // continue for next file
            continue;
        } else if (r == 0 && type == CAN) {
            dbg_msg("[YMRX] cancelled\n");
            return -2;
        } else if (r < 0) {
            // Still waiting; keep sending 'C' periodically
            SYSTICK_DelayMs(50);
        } else {
            // Non-metadata packet while idle ? ignore
            SYSTICK_DelayMs(10);
        }
    }
}

// Transmit a single file via Y-modem.
// Returns 0 on success, -1 on error, -2 if cancelled.
int ymodem_transmit(const char *path) {
    ymodem_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    // Open file read-only
    if (lfs_file_open(&lfs, &ctx.file, path, LFS_O_RDONLY) < 0) {
        dbg_msg("[YMTX] open fail: %s\n", path);
        return -1;
    }

    // Determine file size by reading through, then seek back to start
    ctx.file_size = 0;
    {
        uint8_t tmp[512];
        int n;
        while ((n = lfs_file_read(&lfs, &ctx.file, tmp, sizeof(tmp))) > 0) {
            ctx.file_size += (uint32_t)n;
        }
        if (lfs_file_seek(&lfs, &ctx.file, 0, LFS_SEEK_SET) < 0) {
            dbg_msg("[YMTX] seek start error\n");
            lfs_file_close(&lfs, &ctx.file);
            return -1;
        }
    }

    strncpy(ctx.filename, path, sizeof(ctx.filename) - 1);
    ctx.filename[sizeof(ctx.filename) - 1] = '\0';

    dbg_msg("[YMTX] %s size=%lu\n", ctx.filename, (unsigned long)ctx.file_size);

    // Wait for receiver's 'C'
    int retry = 0;
    while (1) {
        uint8_t c;
        if (ymodem_getc(&c, YM_INITIAL_C_MS) == 0 && c == CRC_REQ) break;
        if (++retry > YM_MAX_RETRY) {
            dbg_msg("[YMTX] timeout waiting 'C'\n");
            lfs_file_close(&lfs, &ctx.file);
            return -1;
        }
        SYSTICK_DelayMs(50);
    }

    // Send block 0 metadata
    {
        uint8_t meta[YM_BLOCK_SIZE_128] = {0};
        size_t p = 0;
        size_t fn = strnlen(ctx.filename, sizeof(ctx.filename));
        memcpy(&meta[p], ctx.filename, fn); p += fn; meta[p++] = '\0';

        char sz[16];
        snprintf(sz, sizeof(sz), "%lu", (unsigned long)ctx.file_size);
        size_t sl = strnlen(sz, sizeof(sz));
        memcpy(&meta[p], sz, sl); p += sl; meta[p++] = '\0';

        if (ymodem_send_packet(SOH, 0, meta, p) != 0) {
            dbg_msg("[YMTX] send block0 failed\n");
            lfs_file_close(&lfs, &ctx.file);
            return -1;
        }

        uint8_t rcv;
        if (ymodem_getc(&rcv, YM_IO_TIMEOUT_MS) != 0 || rcv != ACK) {
            dbg_msg("[YMTX] no ACK for block0\n");
            lfs_file_close(&lfs, &ctx.file);
            return -1;
        }
        if (ymodem_getc(&rcv, YM_IO_TIMEOUT_MS) != 0 || rcv != CRC_REQ) {
            dbg_msg("[YMTX] no 'C' after block0 ACK\n");
            lfs_file_close(&lfs, &ctx.file);
            return -1;
        }
    }

    // Send data blocks
    ctx.blk_num = 1;
    ctx.bytes_done = 0;
    retry = 0;

    while (1) {
        uint8_t buf[YM_BLOCK_SIZE_1K];
        int n = lfs_file_read(&lfs, &ctx.file, buf, sizeof(buf));
        if (n < 0) {
            dbg_msg("[YMTX] read error\n");
            lfs_file_close(&lfs, &ctx.file);
            return -1;
        }
        if (n == 0) break; // EOF

        uint8_t type = (n <= YM_BLOCK_SIZE_128) ? SOH : STX;
        if (ymodem_send_packet(type, ctx.blk_num, buf, (size_t)n) != 0) {
            dbg_msg("[YMTX] send data blk %u failed\n", ctx.blk_num);
            lfs_file_close(&lfs, &ctx.file);
            return -1;
        }

        uint8_t resp;
        if (ymodem_getc(&resp, YM_IO_TIMEOUT_MS) != 0) {
            if (++retry > YM_MAX_RETRY) {
                dbg_msg("[YMTX] timeout waiting ACK\n");
                lfs_file_close(&lfs, &ctx.file);
                return -1;
            }
            if (lfs_file_seek(&lfs, &ctx.file, (long)ctx.bytes_done, LFS_SEEK_SET) < 0) {
                dbg_msg("[YMTX] seek back error\n");
                lfs_file_close(&lfs, &ctx.file);
                return -1;
            }
            continue;
        }

        if (resp == ACK) {
            ctx.bytes_done += (uint32_t)n;
            ctx.blk_num++;
            retry = 0;
        } else if (resp == NAK) {
            if (lfs_file_seek(&lfs, &ctx.file, (long)ctx.bytes_done, LFS_SEEK_SET) < 0) {
                dbg_msg("[YMTX] seek back error\n");
                lfs_file_close(&lfs, &ctx.file);
                return -1;
            }
            if (++retry > YM_MAX_RETRY) {
                dbg_msg("[YMTX] too many NAKs\n");
                lfs_file_close(&lfs, &ctx.file);
                return -1;
            }
        } else if (resp == CAN) {
            dbg_msg("[YMTX] cancelled by receiver\n");
            lfs_file_close(&lfs, &ctx.file);
            return -2;
        }
    }

    // End-of-file: double-EOT handshake
    retry = 0;
    while (1) {
        if (ymodem_putc(EOT) != 0) {
            dbg_msg("[YMTX] EOT send fail\n");
            lfs_file_close(&lfs, &ctx.file);
            return -1;
        }
        uint8_t resp;
        if (ymodem_getc(&resp, YM_IO_TIMEOUT_MS) == 0) {
            if (resp == NAK) {
                dbg_msg("[YMTX] got NAK after EOT, sending EOT again\n");
                if (ymodem_putc(EOT) != 0) {
                    lfs_file_close(&lfs, &ctx.file);
                    return -1;
                }
                if (ymodem_getc(&resp, YM_IO_TIMEOUT_MS) == 0 && resp == ACK) {
                    break;
                }
            } else if (resp == ACK) {
                break;
            } else if (resp == CAN) {
                dbg_msg("[YMTX] cancelled at EOT\n");
                lfs_file_close(&lfs, &ctx.file);
                return -2;
            }
        }
        if (++retry > YM_MAX_RETRY) {
            dbg_msg("[YMTX] no ACK for EOT\n");
            lfs_file_close(&lfs, &ctx.file);
            return -1;
        }
    }

    // After EOT/ACK, receiver sends 'C' to invite next file or end batch
    {
        uint8_t resp;
        if (ymodem_getc(&resp, YM_IO_TIMEOUT_MS) == 0 && resp == CRC_REQ) {
            // Send empty block0 to terminate session
            uint8_t empty[YM_BLOCK_SIZE_128] = {0};
            if (ymodem_send_packet(SOH, 0, empty, YM_BLOCK_SIZE_128) != 0) {
                dbg_msg("[YMTX] send empty block0 failed\n");
                lfs_file_close(&lfs, &ctx.file);
                return -1;
            }
            if (ymodem_getc(&resp, YM_IO_TIMEOUT_MS) != 0 || resp != ACK) {
                dbg_msg("[YMTX] no ACK for empty block0\n");
                lfs_file_close(&lfs, &ctx.file);
                return -1;
            }
        }
    }

    lfs_file_close(&lfs, &ctx.file);
    dbg_msg("[YMTX] done\n");
    return 0;
}


// Optional: CLI entry point using your existing pattern
// int cl_ymodem(int argc, char *argv[]);
// Provide this in your CLI module; link against ymodem.c.
/*
int cl_ymodem(int argc, char *argv[]) {
    dbg_msg("+%s\n", __func__);
    if (argc > 1) {
        dbg_msg("Y-Modem Transmit, file: %s\n", argv[1]);
        return ymodem_transmit(argv[1]);
    } else {
        dbg_msg("Y-Modem Receive\n");
        return ymodem_receive();
    }
}
*/


int cl_ymodem(void) {
   dbg_msg("+%s\n",__func__);
   if(argc > 1) {
      // filename provided, use for transmit
      dbg_msg("Y-Modem Transmit, file: %s\n",argv[1]);
      return ymodem_transmit(argv[1]);
   } else {
      // no parameters provided, assume receive
      dbg_msg("Y-Modem Receive\n");
      return ymodem_receive();
   }
   return 0;
}