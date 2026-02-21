/* ini_store_v2.c - Append-log INI-style storage implementation (single file)
 *
 * Writes are appended to the same `<filename>` as `key=value\n`. Reads parse
 * the file and the last occurrence of a key wins. `ini_compact()` rewrites
 * a compact snapshot and atomically replaces the original via rename.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "littlefs/lfs.h"
#include "littlefs/lfs_interface.h"
#include "ini_store_v2/ini_store_v2.h"

/* Simple limits appropriate for small embedded configs. Increase if needed. */
#define MAX_ENTRIES 128
#define MAX_KEY_LEN 64
#define MAX_VAL_LEN 128

/* Local metrics store (best-effort, not persisted). */
static struct ini_metrics metrics = {0};

/* parse_line: same semantics as v1 */
static int parse_line_local(const char *line, char *key, size_t ksz, char *val, size_t vsz) {
    const char *eq = strchr(line, '=');
    if (!eq) return -1;
    size_t klen = eq - line;
    size_t vlen = strlen(eq + 1);
    if (klen >= ksz || vlen >= vsz) return -1;
    memcpy(key, line, klen);
    key[klen] = '\0';
    // Use snprintf to safely copy and NUL-terminate the value buffer
    snprintf(val, vsz, "%s", eq + 1);
    return 0;
}

/* merge_file_into_map: read `file` lines and update key/value arrays (last wins) */
static int merge_file_into_map(lfs_file_t *file, char keys[][MAX_KEY_LEN], char vals[][MAX_VAL_LEN], int *count) {
    char line[256];
    char kbuf[MAX_KEY_LEN], vbuf[MAX_VAL_LEN];
    while (1) {
        int n = lfs_gets(file, line, sizeof(line));
        if (n == 0) break; // EOF
        if (n < 0) return n;
        // skip comments/blank
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0') continue;
        if (parse_line_local(line, kbuf, sizeof(kbuf), vbuf, sizeof(vbuf)) == 0) {
            // trim trailing whitespace/newline from value
            size_t len = strlen(vbuf);
            while (len > 0 && (vbuf[len-1] == '\n' || vbuf[len-1] == '\r' || vbuf[len-1] == ' ' || vbuf[len-1] == '\t')) vbuf[--len] = '\0';
            // find existing key
            int i;
            for (i = 0; i < *count; ++i) {
                if (strcmp(keys[i], kbuf) == 0) {
                    strncpy(vals[i], vbuf, MAX_VAL_LEN-1);
                    vals[i][MAX_VAL_LEN-1] = '\0';
                    break;
                }
            }
            if (i == *count) {
                if (*count >= MAX_ENTRIES) return -1; // too many entries
                strncpy(keys[*count], kbuf, MAX_KEY_LEN-1);
                keys[*count][MAX_KEY_LEN-1] = '\0';
                strncpy(vals[*count], vbuf, MAX_VAL_LEN-1);
                vals[*count][MAX_VAL_LEN-1] = '\0';
                (*count)++;
            }
        }
    }
    return 0;
}

/* ini_read: read snapshot then overlay log (if present) */
int ini_read(const char *filename, const char *search_key, char *value, size_t vsz) {
    lfs_file_t f;
    char keys[MAX_ENTRIES][MAX_KEY_LEN];
    char vals[MAX_ENTRIES][MAX_VAL_LEN];
    int count = 0;
    // Read the single .ini file (contains snapshot + appended entries)
    if (lfs_file_open(&lfs, &f, filename, LFS_O_RDONLY) >= 0) {
        int rc = merge_file_into_map(&f, keys, vals, &count);
        lfs_file_close(&lfs, &f);
        if (rc < 0) return rc;
    }
    for (int i = 0; i < count; ++i) {
        if (strcmp(keys[i], search_key) == 0) {
            strncpy(value, vals[i], vsz - 1);
            value[vsz - 1] = '\0';
            return 0;
        }
    }
    return -1; // not found
}

/* ini_write: append key=value to filename.log */
int ini_write(const char *filename, const char *key, const char *value) {
    lfs_file_t f;
    // Append directly into the same .ini file to avoid a separate log file
    if (lfs_file_open(&lfs, &f, filename, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND) < 0)
        return -1;
    char line[256];
    int len = snprintf(line, sizeof(line), "%s=%s\n", key, value);
    int w = lfs_file_write(&lfs, &f, line, len);
    lfs_file_close(&lfs, &f);
    if (w < 0) return w;
    metrics.writes++;
    metrics.bytes_written += (uint32_t)w;
    return 0;
}

/* ini_get_uint32 / ini_set_uint32 mirror v1 behaviour */
int ini_get_uint32(const char *filename, const char *key, uint32_t *out) {
    char val[MAX_VAL_LEN];
    if (ini_read(filename, key, val, sizeof(val)) < 0) return -1;
    // trim
    size_t len = strlen(val);
    while (len > 0 && (val[len-1] == '\n' || val[len-1] == '\r' || val[len-1] == ' ' || val[len-1] == '\t')) val[--len] = '\0';
    char *endptr;
    unsigned long tmp = strtoul(val, &endptr, 0);
    if (*endptr != '\0') return -1;
    if (tmp > UINT32_MAX) return -1;
    *out = (uint32_t)tmp;
    return 0;
}

int ini_set_uint32(const char *filename, const char *key, uint32_t value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", value);
    return ini_write(filename, key, buf);
}

/* ini_compact: merge base + log and write compact snapshot; remove log on success */
int ini_compact(const char *filename) {
    lfs_file_t f;
    char keys[MAX_ENTRIES][MAX_KEY_LEN];
    char vals[MAX_ENTRIES][MAX_VAL_LEN];
    int count = 0;
    int rc;
    // Read the current .ini file which may contain appended entries
    if (lfs_file_open(&lfs, &f, filename, LFS_O_RDONLY) >= 0) {
        rc = merge_file_into_map(&f, keys, vals, &count);
        lfs_file_close(&lfs, &f);
        if (rc < 0) return rc;
    }
    // Write tmp snapshot
    if (lfs_file_open(&lfs, &f, "tmp.ini", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) < 0)
        return -1;
    for (int i = 0; i < count; ++i) {
        char line[256];
        int len = snprintf(line, sizeof(line), "%s=%s\n", keys[i], vals[i]);
        if (lfs_file_write(&lfs, &f, line, len) < 0) {
            lfs_file_close(&lfs, &f);
            return -1;
        }
    }
    lfs_file_close(&lfs, &f);

    // Replace original snapshot with compacted file

    // Use atomic rename to replace original with compacted snapshot
    lfs_remove(&lfs, filename);
    if (lfs_rename(&lfs, "tmp.ini", filename) < 0) return -1;

    metrics.compactions++;
    return 0;
}

int ini_get_metrics(const char *filename, struct ini_metrics *out) {
    if (!out) return -1;
    // currently global, file-agnostic; returned as best-effort
    *out = metrics;
    return 0;
}

/* --- Command-line wrappers ---
 * These functions follow the command signature used by the command table
 * (int func(void)) and rely on the global `argv`/`argc` provided by the
 * command_line module. They present simple usage and call the ini API above.
 */

/* Access command_line globals from another translation unit */
extern char *argv[];
extern int argc;

#include "logger/logger.h"

int cl_iniset(void) {
    if (argc < 4) {
        log_msg("Invalid Arg cnt: %d Expected: %d\n", argc - 1, 3);
        log_msg("Usage: iniset <filename> <key> <value>\n");
        return 0;
    }
    const char *filename = argv[1];
    const char *key = argv[2];
    const char *value = argv[3];
    int rc = ini_write(filename, key, value);
    if (rc == 0) {
        log_msg("Wrote %s=%s to %s\n", key, value, filename);
    } else {
        log_msg("Error writing %s to %s (rc=%d)\n", key, filename, rc);
    }
    return rc;
}

int cl_iniget(void) {
    if (argc < 3) {
        log_msg("Invalid Arg cnt: %d Expected: %d\n", argc - 1, 2);
        log_msg("Usage: iniget <filename> <key>\n");
        return 0;
    }
    const char *filename = argv[1];
    const char *key = argv[2];
    char val[MAX_VAL_LEN];
    int rc = ini_read(filename, key, val, sizeof(val));
    if (rc == 0) {
        log_msg("%s=%s\n", key, val);
    } else {
        log_msg("Key '%s' not found in %s (rc=%d)\n", key, filename, rc);
    }
    return rc;
}
