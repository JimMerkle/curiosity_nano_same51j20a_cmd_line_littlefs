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
// Initialization: delete any existing random.ini and recreate it
static void init_ini_file(const char *filename) {
    // Delete existing file if present
    lfs_remove(&lfs, filename);

    // Recreate with predictable contents
    ini_write(filename, "alpha1", "foo");
    ini_write(filename, "beta2", "bar");
    ini_write(filename, "gamma3", "baz");
    ini_write(filename, "delta4", "qux");
    ini_write(filename, "epsilon5", "quux");
    ini_write(filename, "zeta6", "corge");
    ini_write(filename, "eta7", "grault");
    ini_write(filename, "theta8", "garply");
    ini_write(filename, "iota9", "waldo");
    ini_write(filename, "kappa10", "fred");
    ini_write(filename, "lambda11", "plugh");
    ini_write(filename, "mu12", "xyzzy");
    ini_write(filename, "nu13", "thud");
    ini_write(filename, "xi14", "hello");
    ini_write(filename, "omicron15", "world");
    ini_write(filename, "pi16", "test123");
    ini_write(filename, "rho17", "abcdef");
    ini_write(filename, "sigma18", "ghijkl");
    ini_write(filename, "tau19", "mnopqr");
    ini_write(filename, "upsilon20", "stuvwx");
    ini_write(filename, "phi21", "yz");
    ini_write(filename, "chi22", "12345");
    ini_write(filename, "psi23", "67890");
    ini_write(filename, "omega24", "24680");
    ini_write(filename, "key25", "value25");
    ini_write(filename, "key26", "value26");
    ini_write(filename, "key27", "value27");
    ini_write(filename, "key28", "value28");
    ini_write(filename, "key29", "value29");
    ini_write(filename, "key30", "value30");
    ini_write(filename, "rand31", "apple");
    ini_write(filename, "rand32", "banana");
    ini_write(filename, "rand33", "cherry");
    ini_write(filename, "rand34", "date");
    ini_write(filename, "rand35", "elderberry");
    ini_write(filename, "rand36", "fig");
    ini_write(filename, "rand37", "grape");
    ini_write(filename, "rand38", "honeydew");
    ini_write(filename, "rand39", "kiwi");
    ini_write(filename, "rand40", "lemon");
    ini_write(filename, "rand41", "mango");
    ini_write(filename, "rand42", "nectarine");
    ini_write(filename, "rand43", "orange");
    ini_write(filename, "rand44", "papaya");
    ini_write(filename, "rand45", "quince");
    ini_write(filename, "rand46", "raspberry");
    ini_write(filename, "rand47", "strawberry");
    ini_write(filename, "rand48", "tangerine");
    ini_write(filename, "rand49", "ugli");
    ini_write(filename, "rand50", "vanilla");

    log_msg("[INIT] Created fresh %s with 50 entries\n", filename);
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
    uint32_t duration_ms = 10 * 1000; // 10 seconds
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
    
    init_ini_file(fname);
    
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
