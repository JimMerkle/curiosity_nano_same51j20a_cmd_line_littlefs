/*
 * ini_store.c - Simple INI-style key-value storage using LittleFS
 *
 * Theory of Operation:
 * --------------------
 * - Storage format: Each line in the file is "key=value\n".
 * - Only string values are supported. No sections.
 * - Comments: Lines beginning with '#' or ';' are ignored.
 * - Blank lines are ignored.
 * - Reading:
 *     * The file is opened read-only.
 *     * Each line is read using lfs_gets().
 *     * If the line is a comment or blank, it is skipped.
 *     * If the line contains "key=value", the key is compared.
 *     * If it matches, the value is returned.
 * - Writing:
 *     * The file is opened read-only if it exists.
 *     * A temporary file is created.
 *     * All lines are copied to the temp file.
 *       - If the key matches, the line is replaced with the new value.
 *       - Comments and other keys are preserved.
 *     * If the key was not found, a new "key=value" line is appended.
 *     * The original file is replaced with the temp file.
 * - Helpers:
 *     * ini_get_uint32(): Reads a key and parses its value as uint32_t.
 *     * ini_set_uint32(): Writes a uint32_t value as a decimal string.
 *
 * Return conventions:
 * - 0 on success
 * - <0 on error
 * - For read functions, -1 if key not found
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "littlefs/lfs.h"
#include "littlefs/lfs_interface.h"
#include "ini_store/ini_store.h"

// --- parse_line() ---
int parse_line(const char *line, char *key, size_t ksz, char *val, size_t vsz) {
    const char *eq = strchr(line, '=');
    if (!eq) return -1;
    size_t klen = eq - line;
    size_t vlen = strlen(eq + 1);

    if (klen >= ksz || vlen >= vsz) return -1;

    strncpy(key, line, klen);
    key[klen] = '\0';
    strncpy(val, eq + 1, vlen);
    val[vlen] = '\0';
    return 0;
}

// --- ini_read() ---
int ini_read(const char *filename, const char *search_key, char *value, size_t vsz) {
    lfs_file_t file;
    char line[128];
    char key[64], val[64];

    if (lfs_file_open(&lfs, &file, filename, LFS_O_RDONLY) < 0)
        return -1;

    while (1) {
        int n = lfs_gets(&file, line, sizeof(line));
        if (n == 0) break;       // EOF
        if (n < 0) { lfs_file_close(&lfs, &file); return n; }

        // Skip comments and blank lines
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0')
            continue;

        if (parse_line(line, key, sizeof(key), val, sizeof(val)) == 0) {
            if (strcmp(key, search_key) == 0) {
                // Trim trailing newline/whitespace
                size_t len = strlen(val);
                while (len > 0 && (val[len-1] == '\n' || val[len-1] == '\r' ||
                                   val[len-1] == ' '  || val[len-1] == '\t')) {
                    val[--len] = '\0';
                }

                strncpy(value, val, vsz - 1);
                value[vsz - 1] = '\0';
                lfs_file_close(&lfs, &file);
                return 0;
            }
        }
    }

    lfs_file_close(&lfs, &file);
    return -1; // not found
}

// --- ini_write() ---
int ini_write(const char *filename, const char *key, const char *value) {
    lfs_file_t in, out;
    char line[128], kbuf[64], vbuf[64];
    int found = 0;

    // Clean stale temp and open files
    lfs_remove(&lfs, "tmp.ini");
    if (lfs_file_open(&lfs, &in, filename, LFS_O_RDONLY) < 0) {
        // If the file doesn't exist yet, just create it with the key
        if (lfs_file_open(&lfs, &out, "tmp.ini", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) < 0)
            return -1;
        int len = snprintf(line, sizeof(line), "%s=%s\n", key, value);
        lfs_file_write(&lfs, &out, line, len);
        lfs_file_close(&lfs, &out);
        lfs_rename(&lfs, "tmp.ini", filename);
        return 0;
    }
    if (lfs_file_open(&lfs, &out, "tmp.ini", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) < 0) {
        lfs_file_close(&lfs, &in);
        return -1;
    }

    while (1) {
        int n = lfs_gets(&in, line, sizeof(line));
        if (n == 0) break;            // EOF
        if (n < 0) {                   // read error
            lfs_file_close(&lfs, &in);
            lfs_file_close(&lfs, &out);
            return n;
        }

        // Preserve comments/blank lines verbatim
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0') {
            lfs_file_write(&lfs, &out, line, strlen(line));
            continue;
        }

        // Parse and possibly replace
        if (parse_line(line, kbuf, sizeof(kbuf), vbuf, sizeof(vbuf)) == 0) {
            if (!found && strcmp(kbuf, key) == 0) {
                int len = snprintf(line, sizeof(line), "%s=%s\n", key, value);
                lfs_file_write(&lfs, &out, line, len);
                found = 1;
                continue;
            }
        }
        // Copy original line
        lfs_file_write(&lfs, &out, line, strlen(line));
    }

    // Append if not found
    if (!found) {
        int len = snprintf(line, sizeof(line), "%s=%s\n", key, value);
        lfs_file_write(&lfs, &out, line, len);
    }

    lfs_file_close(&lfs, &in);
    lfs_file_close(&lfs, &out);

    // Replace original
    lfs_remove(&lfs, filename);
    lfs_rename(&lfs, "tmp.ini", filename);
    return 0;
}


int ini_get_uint32(const char *filename, const char *key, uint32_t *out) {
    char val[64];
    if (ini_read(filename, key, val, sizeof(val)) < 0)
        return -1;

    // Trim trailing newline and spaces
    size_t len = strlen(val);
    while (len > 0 && (val[len-1] == '\n' || val[len-1] == '\r' || val[len-1] == ' ' || val[len-1] == '\t')) {
        val[--len] = '\0';
    }

    char *endptr;
    unsigned long tmp = strtoul(val, &endptr, 0); // auto base detection
    if (*endptr != '\0') return -1; // invalid number
    if (tmp > UINT32_MAX) return -1;

    *out = (uint32_t)tmp;
    return 0;
}

// --- ini_set_uint32() ---
int ini_set_uint32(const char *filename, const char *key, uint32_t value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", value);
    return ini_write(filename, key, buf);
}
