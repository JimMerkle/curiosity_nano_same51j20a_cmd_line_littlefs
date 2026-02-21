/* ini_store_v2.h - Append-log INI-style storage (compactible)
 *
 * Same public API as ini_store with an additional ini_compact() call
 * and a lightweight metrics struct for future use.
 */
#ifndef INI_STORE_V2_H
#define INI_STORE_V2_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int ini_read(const char *filename, const char *search_key, char *value, size_t vsz);
int ini_write(const char *filename, const char *key, const char *value);
int ini_get_uint32(const char *filename, const char *key, uint32_t *out);
int ini_set_uint32(const char *filename, const char *key, uint32_t value);

/* Command-line helpers (used by command table). Implemented in ini_store_v2.c */
int cl_iniset(void);
int cl_iniget(void);

/* Create a compact snapshot of `filename` by merging the base snapshot
 * and the append-only log (filename + ".log"). After success the log
 * is removed and `filename` contains one line per key with the latest value.
 */
int ini_compact(const char *filename);

/* Lightweight metrics for eventual instrumentation. Values are optional
 * and may be reported by future tooling; implementations may leave them
 * zero when not tracked.
 */
struct ini_metrics {
    uint32_t writes;      /* number of append writes */
    uint32_t compactions; /* number of successful compactions */
    uint32_t bytes_written; /* total bytes appended */
    uint32_t snapshot_size; /* size of current snapshot file in bytes */
    uint32_t entry_count;   /* number of unique keys in snapshot (excluding compacted_size) */
    uint32_t compacted_size; /* value recorded at last compaction, if present */
};

/* Retrieve metrics for a given file (best-effort). Returns 0 on success. */
int ini_get_metrics(const char *filename, struct ini_metrics *out);

#ifdef __cplusplus
}
#endif

#endif /* INI_STORE_V2_H */
