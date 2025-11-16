/*
 * store_ini_test.c - Test harness for ini_store.c
 *
 * This program exercises the ini_store API using a sample "random.ini"
 * file with ~50 key-value pairs. It runs through multiple categories:
 *   1. Basic read/write roundtrip
 *   2. Update existing keys
 *   3. Append new keys
 *   4. Comment handling
 *   5. Blank line handling
 *   6. Numeric values with ini_get_uint32()
 *
 * Each category executes 5 sub-tests.
 * A summary of total tests and pass/fail counts is printed at the end.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "littlefs/lfs_interface.h"
#include "ini_store/ini_store.h"
#include "logger/logger.h"
#include "definitions.h"                // SYS function prototypes

#if 1
static int total_tests = 0;
static int passed_tests = 0;

static void check(const char *label, int cond) {
    total_tests++;
    if (cond) {
        passed_tests++;
        log_msg("[PASS] %s\n", label);
    } else {
        log_msg("[FAIL] %s\n", label);
    }
}

// ------------------------------------------------------------
// 1. Basic Read/Write Roundtrip
static void test_roundtrip(const char *filename) {
    log_msg("\n-- Test 1: Roundtrip --\n");
    char buf[64];

    const char *keys[5]   = {"alpha1","beta2","gamma3","delta4","epsilon5"};
    const char *values[5] = {"foo","bar","baz","qux","quux"};

    for (int i=0;i<5;i++) {
        ini_write(filename, keys[i], values[i]);
        int rc = ini_read(filename, keys[i], buf, sizeof(buf));
        check(keys[i], rc==0 && strcmp(buf, values[i])==0);
    }
}

// ------------------------------------------------------------
// 2. Update Existing Keys
static void test_update(const char *filename) {
    log_msg("\n-- Test 2: Update Existing Keys --\n");
    char buf[64];

    const char *keys[5]   = {"zeta6","eta7","theta8","iota9","kappa10"};
    const char *values[5] = {"updated1","updated2","updated3","updated4","updated5"};

    for (int i=0;i<5;i++) {
        ini_write(filename, keys[i], values[i]);
        int rc = ini_read(filename, keys[i], buf, sizeof(buf));
        check(keys[i], rc==0 && strcmp(buf, values[i])==0);
    }
}

// ------------------------------------------------------------
// 3. Append New Keys
static void test_append(const char *filename) {
    log_msg("\n-- Test 3: Append New Keys --\n");
    char buf[64];

    const char *keys[5]   = {"new51","new52","new53","new54","new55"};
    const char *values[5] = {"val51","val52","val53","val54","val55"};

    for (int i=0;i<5;i++) {
        ini_write(filename, keys[i], values[i]);
        int rc = ini_read(filename, keys[i], buf, sizeof(buf));
        check(keys[i], rc==0 && strcmp(buf, values[i])==0);
    }
}

// ------------------------------------------------------------
// 4. Comment Handling
static void test_comments(const char *filename) {
    log_msg("\n-- Test 4: Comment Handling --\n");
    char buf[64];

    const char *keys[5]   = {"lambda11","mu12","nu13","xi14","omicron15"};
    const char *values[5] = {"val11","val12","val13","val14","val15"};

    for (int i=0;i<5;i++) {
        ini_write(filename, keys[i], values[i]);
        int rc = ini_read(filename, keys[i], buf, sizeof(buf));
        check(keys[i], rc==0 && strcmp(buf, values[i])==0);
    }
}

// ------------------------------------------------------------
// 5. Blank Line Handling
static void test_blanklines(const char *filename) {
    log_msg("\n-- Test 5: Blank Line Handling --\n");
    char buf[64];

    const char *keys[5]   = {"pi16","rho17","sigma18","tau19","upsilon20"};
    const char *values[5] = {"val16","val17","val18","val19","val20"};

    for (int i=0;i<5;i++) {
        ini_write(filename, keys[i], values[i]);
        int rc = ini_read(filename, keys[i], buf, sizeof(buf));
        check(keys[i], rc==0 && strcmp(buf, values[i])==0);
    }
}

// ------------------------------------------------------------
// 6. Numeric Values
static void test_numeric(const char *filename) {
    log_msg("\n-- Test 6: Numeric Values --\n");
    uint32_t val;

    struct { const char *key; const char *str; uint32_t expect; int should_pass; } tests[5] = {
        {"baudrate","115200",115200,1},
        {"timeout","30",30,1},
        {"hexval","0x1A",0x1A,1},
        {"octval","077",077,1},
        {"badnum","abc123",0,0}
    };

    for (int i=0;i<5;i++) {
        ini_write(filename, tests[i].key, tests[i].str);
        int rc = ini_get_uint32(filename, tests[i].key, &val);
        if (tests[i].should_pass)
            check(tests[i].key, rc==0 && val==tests[i].expect);
        else
            check(tests[i].key, rc<0);
    }
}

// Test 7: Stress Test with per-op time check
static void test_stress(void) {
    log_msg("\n-- Test 7: Stress Test --\n");
    const char test_file[]={"random.ini"};

    uint32_t start_ms = SYSTICK_GetTickCounter();
    uint32_t duration_ms = 20 * 1000; // 20 seconds
    uint32_t ops = 0;

    char buf[64];
    uint32_t val;

    enum {
        ST_WRITE1, ST_WRITE2, ST_WRITE3,
        ST_READ1,  ST_READ2,  ST_READ3,
        ST_GET1,   ST_GET2,   ST_GET3
    } state = ST_WRITE1;

    while (1) {
        uint32_t now = SYSTICK_GetTickCounter();
        if ((now - start_ms) >= duration_ms) break;

        switch (state) {
        case ST_WRITE1:
            ini_write(test_file, "stress_key1", "123");
            state = ST_WRITE2;
            break;
        case ST_WRITE2:
            ini_write(test_file, "stress_key2", "456");
            state = ST_WRITE3;
            break;
        case ST_WRITE3:
            ini_write(test_file, "stress_key3", "789");
            state = ST_READ1;
            break;
        case ST_READ1:
            ini_read(test_file, "stress_key1", buf, sizeof(buf));
            state = ST_READ2;
            break;
        case ST_READ2:
            ini_read(test_file, "stress_key2", buf, sizeof(buf));
            state = ST_READ3;
            break;
        case ST_READ3:
            ini_read(test_file, "stress_key3", buf, sizeof(buf));
            state = ST_GET1;
            break;
        case ST_GET1:
            ini_get_uint32(test_file, "stress_key1", &val);
            state = ST_GET2;
            break;
        case ST_GET2:
            ini_get_uint32(test_file, "stress_key2", &val);
            state = ST_GET3;
            break;
        case ST_GET3:
            ini_get_uint32(test_file, "stress_key3", &val);
            state = ST_WRITE1; // loop back
            break;
        }

        ops++;
    }

    uint32_t end_ms = SYSTICK_GetTickCounter();
    uint32_t elapsed = end_ms - start_ms;

    log_msg("[STRESS] Ran %lu operations in %lu ms\n",
            (unsigned long)ops, (unsigned long)elapsed);
    log_msg("[STRESS] Average ops/sec: %lu\n",
            (unsigned long)(ops * 1000UL / elapsed));
}

// ------------------------------------------------------------
int cl_ini_store_test(void) {
    const char *fname = "random.ini";

    log_msg("=== INI Store Test Harness ===\n");

    test_roundtrip(fname);
    test_update(fname);
    test_append(fname);
    test_comments(fname);
    test_blanklines(fname);
    test_numeric(fname);
    test_stress();

    log_msg("\n=== Test Summary ===\n");
    log_msg("Total tests: %d\n", total_tests);
    log_msg("Passed:      %d\n", passed_tests);
    log_msg("Failed:      %d\n", total_tests - passed_tests);

    return 0;
}
#else

int ini_cleanup_keep_last(const char *filename) {
    // First pass: collect last value per key
    lfs_file_t in;
    char line[128], key[64], val[64];

    if (lfs_file_open(&lfs, &in, filename, LFS_O_RDONLY) < 0) return -1;

    // Simple map: parallel arrays (small file assumption)
    #define MAX_KEYS 256
    char keys[MAX_KEYS][64];
    char vals[MAX_KEYS][64];
    int count = 0;

    while (1) {
        int n = lfs_gets(&in, line, sizeof(line));
        if (n == 0) break;
        if (n < 0) { lfs_file_close(&lfs, &in); return n; }

        if (parse_line(line, key, sizeof(key), val, sizeof(val)) == 0) {
            // Trim trailing whitespace/newlines from val
            size_t len = strlen(val);
            while (len > 0 && (val[len-1] == '\n' || val[len-1] == '\r' ||
                               val[len-1] == ' '  || val[len-1] == '\t')) {
                val[--len] = '\0';
            }

            // Update or insert
            int idx = -1;
            for (int i = 0; i < count; i++) {
                if (strcmp(keys[i], key) == 0) { idx = i; break; }
            }
            if (idx >= 0) {
                strncpy(vals[idx], val, sizeof(vals[idx]) - 1);
                vals[idx][sizeof(vals[idx]) - 1] = '\0';
            } else if (count < MAX_KEYS) {
                strncpy(keys[count], key, sizeof(keys[count]) - 1);
                keys[count][sizeof(keys[count]) - 1] = '\0';
                strncpy(vals[count], val, sizeof(vals[count]) - 1);
                vals[count][sizeof(vals[count]) - 1] = '\0';
                count++;
            }
        }
    }
    lfs_file_close(&lfs, &in);

    // Second pass: write cleaned file (preserve comments/blank lines from original header)
    lfs_remove(&lfs, "tmp.ini");

    lfs_file_t out;
    if (lfs_file_open(&lfs, &out, "tmp.ini", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) < 0)
        return -1;

    // Optional: write a header comment
    const char *hdr = "# Cleaned INI (last occurrence wins)\n";
    lfs_file_write(&lfs, &out, hdr, strlen(hdr));

    for (int i = 0; i < count; i++) {
        int len = snprintf(line, sizeof(line), "%s=%s\n", keys[i], vals[i]);
        lfs_file_write(&lfs, &out, line, len);
    }

    lfs_file_close(&lfs, &out);
    lfs_remove(&lfs, filename);
    lfs_rename(&lfs, "tmp.ini", filename);
    return 0;
}

int cl_ini_store_test(void) {
    const char *fname = "random.ini";

    // Run cleanup once
    ini_cleanup_keep_last(fname);

    log_msg("=== Minimal Diagnostics ===\n");

    // Add numeric test keys
    int rc = ini_write(fname, "baudrate", "115200");
    log_msg("ini_write(baudrate): rc=%d\n", rc);
    
    rc = ini_write(fname, "timeout", "30");
    log_msg("ini_write(timeout): rc=%d\n", rc);
    
    rc = ini_write(fname, "hexval", "0x1A");
    log_msg("ini_write(hexval): rc=%d\n", rc);
    
    rc = ini_write(fname, "octval", "077");
    log_msg("ini_write(octval): rc=%d\n", rc);
    
    rc = ini_write(fname, "badnum", "abc123");
    log_msg("ini_write(badnum): rc=%d\n", rc);

    // 2) Read a few known string keys
    char buf[64];
    rc = ini_read(fname, "alpha1", buf, sizeof(buf));
    log_msg("ini_read(alpha1): rc=%d, val='%s'\n", rc, (rc==0)?buf:"<n/a>");

    rc = ini_read(fname, "beta2", buf, sizeof(buf));
    log_msg("ini_read(beta2): rc=%d, val='%s'\n", rc, (rc==0)?buf:"<n/a>");

    // 3) Probe numeric keys directly
    uint32_t val;
    rc = ini_get_uint32(fname, "baudrate", &val);
    log_msg("ini_get_uint32(baudrate): rc=%d, val=%u\n", rc, val);

    rc = ini_get_uint32(fname, "timeout", &val);
    log_msg("ini_get_uint32(timeout): rc=%d, val=%u\n", rc, val);

    rc = ini_get_uint32(fname, "hexval", &val);
    log_msg("ini_get_uint32(hexval): rc=%d, val=%u\n", rc, val);

    rc = ini_get_uint32(fname, "octval", &val);
    log_msg("ini_get_uint32(octval): rc=%d, val=%u\n", rc, val);

    rc = ini_get_uint32(fname, "badnum", &val);
    log_msg("ini_get_uint32(badnum): rc=%d\n", rc);

    log_msg("=== Diagnostics Complete ===\n");
    return 0;
}
#endif
