# virus_detector

Basic Linux antivirus scanner written in C++23.

The scanner walks the filesystem, checks file contents against known signatures,
and moves detected files into quarantine.

File scanning is parallelized with a thread pool, and signature matching uses
the Aho-Corasick algorithm so all signatures can be searched in a single pass
over each file.

---

## Requirements

- g++ with C++23 support
- libsqlite3
- make

SQLiteCpp is included under `external/`, so no additional download is required.

---

## Build

```bash
make
```

Builds:

```text
./av
```

Clean build files:

```bash
make clean
```

Build a Debian package:

```bash
make deb
```

or:

```bash
make deb VERSION=0.1.0
```

The package is created under:

```text
dist/
```

After installation, the binary is available as:

```text
av
```

The Debian package also installs a systemd service that runs `scan-all`
once on system boot.

---

## Usage

After building:

```bash
./av scan-all
./av scan /path/to/file-or-directory
./av q-list
./av restore <id>
./av restore-all
./av delete <id>
./av help
```

Commands:

* `scan-all` — scans from `/`
* `scan <path>` — scans one file or directory
* `q-list` — lists quarantined files
* `restore <id>` — restores one quarantined file
* `restore-all` — restores all quarantined files
* `delete <id>` — permanently removes a quarantined file
* `help` — prints available commands

---

## How scanning works

The scan has two main levels:

1. Finding files on the filesystem
2. Scanning the contents of each file

### 1. Filesystem traversal

`FileTreeWalker` walks the requested directory tree using deterministic DFS.

For every discovered path:

1. Excluded paths are skipped.
2. Symbolic links are not followed.
3. File metadata such as size and modification time is collected.
4. The cache is checked before submitting expensive work to the thread pool.

The important flow is:

```text
File discovered
      |
      v
Cache lookup
   /      \
 HIT      MISS
  |         |
Reuse     ThreadPool
verdict       |
              v
        Scan file contents
```

Cache lookup is done before enqueueing because it is cheap.
Only cache misses are sent to the worker threads.

This keeps the workers focused on expensive file I/O and signature matching.

---

## Parallel file scanning

The scanner uses a fixed thread pool with 16 workers.

The filesystem traversal thread discovers files and submits only cache misses
into a bounded task queue.

```text
FileTreeWalker
      |
      v
bounded task queue
      |
      v
16 worker threads
```

The queue is bounded so traversal cannot run far ahead of the workers and grow
memory usage without limit.

Workers sleep while there is no work and wake when tasks become available.

When the queue is full, traversal waits until a worker removes a task.

At the end of the scan, the pipeline waits until:

```text
task queue is empty
AND
no worker is currently processing a file
```

---

## File scanning algorithm

Each worker scans one file using `FileProcessor`.

Files are read in chunks instead of loading the entire file into memory.

This allows the scanner to handle very large files.

The automaton state is preserved between chunks, so a signature that begins at
the end of one chunk and finishes at the beginning of the next chunk is still
detected.

Example:

```text
chunk 1: ........MALW
chunk 2: ARE_SIGNATURE.......
                 ^
```

The signature can still match across the chunk boundary.

---

## Aho-Corasick signature matching

Before filesystem scanning begins, all signatures are loaded and one
Aho-Corasick automaton is built.

Without Aho-Corasick, scanning a file against many signatures could look like:

```text
scan file for signature 1
scan file for signature 2
scan file for signature 3
...
```

Aho-Corasick combines all signatures into one automaton.

The file is then processed approximately once:

```text
all signatures
      |
      v
build automaton once
      |
      v
scan file bytes
      |
      v
all matches
```

The automaton contains:

* trie transitions
* failure links
* output states for matched signatures

The automaton is read-only after construction and is shared safely between
worker threads.

---

## Scan results

A successful file scan produces one of two verdicts:

```text
Clean
Malicious
```

If the file cannot be opened or read, the scan is treated as an error.

An unreadable file is not considered clean and is not written to the cache.

The summary tracks failed scans separately.

---

## Cache

The cache is stored in SQLite under:

```text
runtime/cache/
```

Its purpose is to avoid scanning files that have not changed.

A cache entry contains information such as:

```text
path
file size
last modification time
signature version
verdict
generation
```

When a file is discovered, the current metadata is compared with the cached
metadata.

If they match:

```text
Cache Hit
→ reuse previous verdict
→ do not enqueue a worker task
```

If they differ:

```text
Cache Miss
→ scan the file again
```

Cache updates are first written to an in-memory map for fast lookup.

A separate `CacheWriter` thread writes updates to SQLite in batches.

```text
workers
   |
   v
cache update queue
   |
  100 updates
   |
   v
CacheWriter
   |
   v
SQLite transaction
```

Any remaining updates below the batch size are written when the scan finishes.

---

## Cache generation cleanup

Each completed full-system scan has a generation number.

A file seen during the current scan receives the current generation.

Example:

```text
scan generation = 43

a.txt -> 43
b.txt -> 43
c.txt -> 42
```

After a successful `scan-all`, entries that still belong to older generations
were not seen during the current full scan and can be removed from the cache.

Generation cleanup is only committed after a successful scan.

If the process crashes before completion, stale cache entries are not removed.

This prevents an interrupted scan from deleting valid cache information.

---

## Resume after interruption

The scanner stores a checkpoint under:

```text
runtime/resume/
```

The traversal order is deterministic, so the checkpoint records where the scan
should continue.

If a scan is interrupted:

```text
scan running
     |
     X process stopped
     |
checkpoint remains
```

The next scan of the same root can continue from that point instead of
restarting from the beginning.

Checkpoint writes are atomic:

```text
write temporary file
      |
      v
rename temporary file
```

This avoids leaving a partially written checkpoint if the process stops during
a save.

Resume state is only marked complete once discovery and all queued work have
finished.

---

## Exclusions

User exclusions are configured in:

```text
config/exclude.txt
```

Only absolute paths are accepted.

The scanner also excludes system paths such as:

```text
/proc
/sys
/dev
/run
/tmp
```

Project/runtime paths are excluded so the scanner does not scan its own cache,
quarantine, build output, or repository metadata.

Symbolic links are not followed to avoid loops and unexpected traversal.

---

## Quarantine

When a file is detected as malicious:

```text
Malicious verdict
      |
      v
move file to quarantine
      |
      v
save metadata
```

Quarantine metadata includes:

```text
id
original path
quarantine path
matched signatures
file size
quarantine time
original permissions
```

The original permissions are saved before moving the file.

During restore, those permissions are reapplied so the restored file keeps its
original mode even if the move required a copy across filesystems.

Available commands:

```bash
./av q-list
./av restore <id>
./av restore-all
./av delete <id>
```

Quarantine metadata is written atomically using a temporary file followed by
rename.

---

## Configuration

### Signatures

```text
config/signatures.txt
```

One signature per line.

Example:

```text
MALWARE_SIGNATURE_1
MALWARE_SIGNATURE_2
```

Lines beginning with `#` are ignored.

Signatures can be edited without recompiling.

When the signature file changes, existing cache entries are no longer trusted
for matching against the new signature set.

### Exclusions

```text
config/exclude.txt
```

One absolute path per line.

Lines beginning with `#` are ignored.

---

## Runtime files

All mutable state is stored under:

```text
runtime/
```

| Directory     | Purpose                           |
| ------------- | --------------------------------- |
| `cache/`      | SQLite cache and generation state |
| `resume/`     | interrupted scan checkpoint       |
| `quarantine/` | quarantined files and metadata    |
| `logs/`       | runtime logs                      |

These files are not committed to git.

---

## Tests

Run:

```bash
./tests/test_main.sh
```

The test suite creates an isolated sandbox outside the repository.

It uses its own:

```text
config/
runtime/
test files
```

and removes them after the run.

The real project configuration and runtime state are not modified.

Tests cover areas including:

* clean scanning
* cache hits and invalidation
* malicious detection
* quarantine / restore / delete
* resume after interruption
* cross-chunk signature matching
* exclusions
* symbolic links
* large files
* multiple signatures in one file
* permission preservation
* real `scan-all`
* corrupted cache/quarantine metadata
* cache generation cleanup
* crash safety during generation handling

---

## Project layout

```text
Application/
    program flow and command dispatch

CLI/
    command-line parsing

Scan/
    scan pipeline
    filesystem traversal
    Aho-Corasick automaton
    file processing

Cache/
    in-memory cache
    SQLite persistence
    asynchronous batch writer
    stale-entry cleanup

Quarantine/
    quarantine
    restore
    delete
    metadata persistence

Resume/
    scan checkpoint and resume logic

Exclude/
    exclusion set
    path filtering
    scan-root validation

Signature/
    signature loading

ThreadPool/
    bounded worker pool

Console/
    user output

Logger/
    runtime logging

Performance/
    execution timing

config/
    signatures and exclusions

tests/
    integration tests
```

---

## High-level architecture

```text
main
 |
 v
Application
 |
 v
ScanPipeline
 |
 +--> load signatures
 |       |
 |       v
 |   build Aho-Corasick
 |
 +--> load cache / quarantine / resume / excludes
 |
 v
FileTreeWalker
 |
 v
discovered file
 |
 v
cache lookup
 |              \
 HIT             MISS
 |                |
 v                v
reuse verdict   ThreadPool
                  |
                  v
             FileProcessor
                  |
                  v
            Aho-Corasick
                  |
                  v
             scan result
                  |
       +----------+-----------+
       |                      |
      Clean                Malicious
       |                      |
       v                      v
     cache                quarantine
       \                      /
        +---------+----------+
                  |
                  v
                resume
```

The central idea is:

**Traversal finds files, the cache removes unnecessary work, the thread pool
parallelizes expensive scans, Aho-Corasick matches all signatures efficiently,
and quarantine/resume/cache handle persistent state around the scan.**

---

## Final Note

I really enjoyed working on this assignment. I put a lot of thought into the
design and tried to keep the implementation simple while still handling
performance, concurrency, caching, recovery, and edge cases carefully.

Thank you for taking the time to review my work. I look forward to discussing
the project and the decisions behind it.
