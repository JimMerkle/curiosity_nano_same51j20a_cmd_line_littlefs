// file: simple_ini.c
/*
How It Works
ini_read:
  Seeks to start of file.
  Reads line by line with lfs_gets.
  Parses key=value.
  Returns value if key matches.

ini_write:
  Opens original file.
  Creates a temporary file.
  Copies all lines, replacing the target key if found.
  If not found, appends new key=value.
  Replaces original file with temp.
*/

#include <stdio.h>
#include <string.h>
#include "lfs.h"

// Assume lfs_gets() is the fixed version we discussed earlier.

// Parse "key=value" line into key and value strings.
// Returns 0 on success, -1 if malformed.
static int parse_line(const char *line, char *key, size_t ksz, char *val, size_t vsz) {
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

// Read value for a given key. Returns 0 on success, <0 on error or not found.
int ini_read(lfs_file_t *file, const char *search_key, char *value, size_t vsz) {
    char line[128];
    char key[64], val[64];

    // rewind file
    lfs_file_seek(&lfs, file, 0, LFS_SEEK_SET);

    while (1) {
        int n = lfs_gets(file, line, sizeof(line));
        if (n == 0) break;       // EOF
        if (n < 0) return n;     // error

        if (parse_line(line, key, sizeof(key), val, sizeof(val)) == 0) {
            if (strcmp(key, search_key) == 0) {
                strncpy(value, val, vsz - 1);
                value[vsz - 1] = '\0';
                return 0;
            }
        }
    }
    return -1; // not found
}

// Write or update a key=value pair.
// Simple approach: read entire file, rewrite to temp buffer, replace file.
int ini_write(const char *filename, const char *key, const char *value) {
    lfs_file_t file;
    char line[128], kbuf[64], vbuf[64];
    int found = 0;

    // Open original file for reading
    if (lfs_file_open(&lfs, &file, filename, LFS_O_RDONLY) < 0) {
        // If file doesn't exist, just create new
        if (lfs_file_open(&lfs, &file, filename, LFS_O_WRONLY | LFS_O_CREAT) < 0)
            return -1;
        int len = snprintf(line, sizeof(line), "%s=%s\n", key, value);
        lfs_file_write(&lfs, &file, line, len);
        lfs_file_close(&lfs, &file);
        return 0;
    }

    // Create temp file
    lfs_file_t tmp;
    if (lfs_file_open(&lfs, &tmp, "tmp.ini", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) < 0) {
        lfs_file_close(&lfs, &file);
        return -1;
    }

    // Copy lines, replacing key if found
    while (1) {
        int n = lfs_gets(&file, line, sizeof(line));
        if (n == 0) break;       // EOF
        if (n < 0) { lfs_file_close(&lfs, &file); lfs_file_close(&lfs, &tmp); return n; }

        if (parse_line(line, kbuf, sizeof(kbuf), vbuf, sizeof(vbuf)) == 0 &&
            strcmp(kbuf, key) == 0) {
            // Replace line
            int len = snprintf(line, sizeof(line), "%s=%s\n", key, value);
            lfs_file_write(&lfs, &tmp, line, len);
            found = 1;
        } else {
            // Copy original line
            lfs_file_write(&lfs, &tmp, line, strlen(line));
            lfs_file_write(&lfs, &tmp, "\n", 1);
        }
    }

    if (!found) {
        // Append new key=value
        int len = snprintf(line, sizeof(line), "%s=%s\n", key, value);
        lfs_file_write(&lfs, &tmp, line, len);
    }

    lfs_file_close(&lfs, &file);
    lfs_file_close(&lfs, &tmp);

    // Replace original file with temp
    lfs_remove(&lfs, filename);
    lfs_rename(&lfs, "tmp.ini", filename);

    return 0;
}
