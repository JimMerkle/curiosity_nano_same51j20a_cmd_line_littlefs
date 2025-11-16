// file: ini_store.h
#ifndef INI_STORE_H
#define INI_STORE_H

#include <stdint.h>
#include "littlefs/lfs.h"

// Parse "key=value" line into key and value strings.
// Internal helper, not usually called directly.
int parse_line(const char *line, char *key, size_t ksz, char *val, size_t vsz);

// Read a string value for a given key.
// Returns 0 on success, <0 on error or not found.
int ini_read(const char *filename, const char *search_key, char *value, size_t vsz);

// Write or update a key=value pair (string).
// Returns 0 on success, <0 on error.
int ini_write(const char *filename, const char *key, const char *value);

// Read a uint32_t value for a given key.
// Returns 0 on success, <0 on error or not found.
int ini_get_uint32(const char *filename, const char *key, uint32_t *out);

// Write a uint32_t value for a given key.
// Returns 0 on success, <0 on error.
int ini_set_uint32(const char *filename, const char *key, uint32_t value);

#endif // INI_STORE_H
