# Virus Detector

A modular C++23 antivirus scanner for Linux. It scans files by signature matching, quarantines malicious files, caches clean results, supports resume after interruption, and runs file processing on a thread pool.

## Features

- **On-demand scan** — scan a path or the whole filesystem (`scan` / `scan-all`)
- **Signature detection** — Aho-Corasick string matching; signatures live in `config/signatures.txt` (no rebuild needed to update them)
- **Quarantine** — move infected files aside; restore or delete by id
- **Cache** — skip unchanged clean files; flush to disk every 100 updates
- **Resume** — checkpoint tracks `next_unfinished_path`; continue after a crash
- **Exclusions** — user paths in `config/exclude.txt` plus built-in system paths (`/proc`, `/sys`, `/dev`, `/run`, `/tmp`)
- **Logging** — one timestamped log file per run under `runtime/logs/`
- **Thread pool** — bounded queue; workers scan files in parallel
- **Symlink safety** — symbolic links are skipped (no follow)

## Build

Requires a C++23 compiler (`g++` 13+) and Make.

```bash
make
```

Binary:

```text
build/bin/av_scanner
```

Clean:

```bash
make clean
```

## Usage

```bash
./build/bin/av_scanner help
./build/bin/av_scanner scan <path>
./build/bin/av_scanner scan-all
./build/bin/av_scanner quarantine-list
./build/bin/av_scanner restore <id>
./build/bin/av_scanner delete <id>
```

Example:

```bash
./build/bin/av_scanner scan ./test_scan
```

## Project layout

```text
.
├── Application/         CLI orchestration (run commands)
├── CLI/                 Command parsing and console output
├── Cache/               Scan-result cache (JSON repository)
├── Common/              Shared types (Error, OperationResult, FileVerdict)
├── Exclude/             Exclusion list + built-in system paths
├── Logger/              Thread-safe file logger
├── Quarantine/          Quarantine manager, FileMover, metadata repository
├── Resume/              Checkpoint + ProgressTracker
├── Scanner/
│   ├── FileProcessor    Single-file cache/scan/quarantine
│   ├── FileEnumerator/  Sorted DFS + SortedDirectoryReader
│   ├── FileScanner/     Chunked signature scan
│   ├── Automaton/       Aho-Corasick
│   └── SignatureManager/
├── ThreadPool/          Worker pool with bounded queue
├── config/              signatures.txt, exclude.txt
├── runtime/             logs, cache, resume, quarantine (gitignored data)
├── external/            Vendored nlohmann/json
├── Makefile
└── main.cpp
```

Build output:

```text
build/
├── obj/     object files
├── dep/     dependency files
└── bin/     av_scanner
```

## Configuration

### Signatures — `config/signatures.txt`

One signature string per line. Lines starting with `#` are comments.

### Exclusions — `config/exclude.txt`

Absolute paths only (for now). System folders are always excluded in code.

## Runtime data

Created automatically under `runtime/`:

| Path | Purpose |
|------|---------|
| `runtime/logs/` | Per-run log files |
| `runtime/cache/cache.json` | Clean-file cache |
| `runtime/resume/checkpoint.json` | Resume checkpoint |
| `runtime/quarantine/` | Quarantined files + `metadata.json` |

These data files are ignored by git (directories kept via `.gitkeep`).

## Architecture notes

```text
FileEnumerator (sorted DFS)
    → ProgressTracker::registerTask
    → ThreadPool::enqueue
    → FileProcessor::process  (Cache / FileScanner / Quarantine)
    → ProgressTracker::markCompleted
```

**Error policy**

- Expected per-file failures → `Error` / `OperationResult`, log, continue
- Critical init failures → stop the command
- Unexpected exceptions → caught at worker and `main` boundaries

**Flush policy**

- Cache: every 100 dirty updates (safe to lose some on crash → only re-scan cost)
- Checkpoint: save only when the resume frontier (`next_unfinished_path`) moves, plus forced flush at end of scan

## Requirements coverage

| Assignment requirement | Implementation |
|------------------------|----------------|
| Scan cache | `CacheManager` + JSON repository |
| Scan path / full disk | `scan` / `scan-all` |
| Quarantine restore/delete | CLI + `QuarantineManager` |
| String matching without rebuild | `SignatureManager` + Aho-Corasick |
| Logging | `Logger` |
| Resume | `ProgressTracker` + checkpoint |
| Exclusions | `ExcludeManager` + system list |

## License

Homework / educational project.
