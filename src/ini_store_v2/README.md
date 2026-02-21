# ini_store_v2

Append-log INI storage for LittleFS (single-file strategy)

Purpose
- Provide a lightweight key=value store using a single `.ini` file with append-only writes and periodic compaction to reduce flash wear.

Files
- `ini_store_v2.h` — public API and metrics struct
- `ini_store_v2.c` — implementation: `ini_read`, `ini_write`, `ini_get_uint32`, `ini_set_uint32`, `ini_compact`, `ini_get_metrics`, plus `cl_iniset`/`cl_iniget` CLI wrappers

Behavior
- Writes are appended to the same `.ini` file as `key=value\n` records.
- Reads scan the file; the last occurrence of a key wins.
- `ini_compact()` rewrites a compact snapshot and atomically replaces the original file using LittleFS rename.

Compaction metadata (`compacted_size`)
- A compacted snapshot records a metadata key `compacted_size=<N>` to help determine how much new data has been appended since the last compaction.
- By convention this README treats `compacted_size` as a metadata entry that does not count as a user key when computing `entry_count`.
- Implementations may place `compacted_size` at the start or end of the file; the important rule is callers must ignore that entry when counting user keys.

Metrics and policy
- `ini_get_metrics(filename, out)` returns (best-effort):
  - `snapshot_size` — current file size in bytes
  - `compacted_size` — value recorded in file (0 if absent)
  - `entry_count` — number of unique user keys (excluding `compacted_size`)
  - `writes`, `compactions`, `bytes_written` — global, best-effort counters
- Derived value: `bytes_appended = snapshot_size - compacted_size` (treat `compacted_size`==0 conservatively)

Typical compact policies
- Compact when any of the following hold:
  - `bytes_appended >= 4096`
  - `bytes_appended >= snapshot_size / 2`
  - `writes_since_compaction >= 100` (if tracked)
  - Manual: user runs `inicompact <file>` CLI
  - Boot/maintenance: run a quick check on startup or during idle time

CLI
- `iniset <filename> <key> <value>` — append `key=value` to file (implemented as `cl_iniset`).
- `iniget <filename> <key>` — read and print the latest value (implemented as `cl_iniget`).
- `inicompact <filename>` — (recommended) provide a wrapper to call `ini_compact()` when needed.

Notes and next steps
- `ini_read()` keeps its simple semantics (returns success/failure and writes the value into caller buffer) — do not overload it to return metrics.
- Prefer using `ini_get_metrics()` for compaction decisions.
- Consider adding a persistent per-file `writes_since_compaction` or a small journal if more precise counting is required.
- Tests: add smoke tests that exercise many appends and then `ini_compact()` validating `entry_count` and file size reduction.

Contact
- Implementation authored within this workspace; open an issue or request if you prefer `compacted_size` placed at the end of the file or different compaction thresholds.
