# C++ File Indexer

A C++ file indexing system that processes large text/log files using fixed-size buffers and supports efficient word-frequency analysis.

## Features

- Uses bounded I/O and pipeline buffers for large files
- Keeps in-flight chunk memory bounded; the word index itself grows with unique vocabulary
- Supports incremental indexing
- Provides word-frequency queries
- Supports Top-K and Difference queries
- Modular design using separate reader, tokenizer, index, query, and CLI components

## Project Structure

```text
.
├── Makefile
├── src/
│   ├── main.cpp
│   ├── cli.cpp
│   ├── cli.h
│   ├── reader.cpp
│   ├── reader.h
│   ├── tokenizer.cpp
│   ├── tokenizer.h
│   ├── index.cpp
│   ├── index.h
│   ├── process_indexer.cpp
│   ├── process_indexer.h
│   ├── persistent_index.cpp
│   ├── persistent_index.h
│   ├── queries.cpp
│   └── queries.h
├── data/
├── build/
├── tests/
│   ├── bounded_queue_test.cpp
│   ├── index_pipeline_test.cpp
│   ├── process_indexer_test.cpp
│   └── persistent_index_test.cpp
└── benchmarks/
    ├── benchmark_readers.sh
    ├── benchmark_memory.sh
    └── benchmark_workers.sh
```

## Build and test

The project requires a POSIX environment, a C++17 compiler, and `make`.

```bash
make
make test
```

The build enables pthread support. `make test` runs bounded-queue, pipeline-boundary, process-protocol, and persistent-index replacement tests.

## Project report

A detailed systems-design report, including benchmark plots, is available as [PDF](docs/file_indexer_report.pdf). Its LaTeX source is [file_indexer_report.tex](docs/file_indexer_report.tex); rebuild it with Tectonic when available:

```bash
tectonic --outdir docs docs/file_indexer_report.tex
```

## Reader modes

`read` is the default reader mode and uses POSIX `open()`, `read()`, and `close()` with the configured fixed-size buffer.

`mmap` maps a regular file privately with `mmap()` and presents buffer-sized chunks directly from the mapping. It avoids copying those chunks into the reader buffer, but it is intentionally unavailable for streams, pipes, and other non-regular files.

Select a mode without changing query semantics:

```bash
./build/file-indexer --query word --reader mmap --file data/log.txt --version v1 --buffer 1024 --word error
```

## Parallel indexing

`--workers N` selects the number of parsing/indexing workers (default: `1`). A single producer reads the file and queues chunks ending at token delimiters; workers tokenize independent chunks into private indexes, then the caller merges them after all workers finish. The queue holds at most eight chunks, which provides backpressure to the reader instead of allowing unbounded queued input.

```bash
./build/file-indexer --query word --workers 4 --file data/log.txt --version v1 --buffer 1024 --word error
```

The synchronization policy keeps the hot index update path lock-free: each worker exclusively owns its partial `WordIndex`. The queue uses a mutex plus `notEmpty`/`notFull` condition variables for handoff and backpressure, while worker-error recording is mutex-protected. Joining workers establishes the handoff before the final serial merge.

## Process-isolated indexing

Add `--process-indexer` to run each version build in a child process. The parent remains the query engine; it uses `fork()`, `exec()`, and a pipe to receive the completed frequency index. The parent checks the child status with `waitpid()` before using the result.

```bash
./build/file-indexer --query word --process-indexer --workers 4 --file data/log.txt --version v1 --buffer 1024 --word error
```

This mode isolates parsing/indexing failures and provides a natural boundary for a future persistent-index service. It serializes the full index through the pipe, so use the default in-process mode when throughput is the priority.

## Crash-safe persistent versions

Pass `--index-dir <path>` while building a version to save it. A save writes a temporary file in that directory, `fsync()`s it, atomically replaces the version file with `rename()`, then `fsync()`s the directory on Linux so the rename is durable across a crash.

```bash
./build/file-indexer --query word --file data/log.txt --version v1 --buffer 1024 --index-dir indexes --word error
./build/file-indexer --query word --load-index --index-dir indexes --version v1 --word error
```

`--load-index` is explicit: it never silently replaces a requested source file with a potentially stale persisted version. Version names are hex-encoded in filenames, so names cannot escape the selected index directory.

## Reader benchmark

Build the binary, then compare the end-to-end index construction time for both modes:

```bash
./benchmarks/benchmark_readers.sh ./build/file-indexer data/log.txt 1024 5
```

The benchmark reports end-to-end indexing latency and throughput. It uses a missing word lookup so each run still builds the complete index.

## Worker scaling benchmark

Measure the full indexing pipeline at 1, 2, 4, and 8 workers. The default reader is `mmap`; pass `read` as the third argument to compare its scaling separately. The script reports average time and speedup relative to one worker.

```bash
./benchmarks/benchmark_workers.sh ./build/file-indexer data/log.txt mmap 1024 5
```

Expect scaling to plateau when the single producer, memory bandwidth, token distribution, or the final merge becomes the bottleneck.

## Peak-memory benchmark

Measure end-to-end latency, throughput, and peak resident memory for an in-process build. On macOS it uses `/usr/bin/time -l`; on Linux it uses `/usr/bin/time -v`.

```bash
./benchmarks/benchmark_memory.sh ./build/file-indexer data/log.txt mmap 4 1024
```

The reported memory unit is platform-specific (`bytes` on macOS, `KiB` on Linux). In restricted environments where the timing tool cannot read memory counters, the script reports memory as unavailable while retaining latency and throughput. Process-isolated mode is intentionally excluded because the parent and child have separate resident-memory accounting.

## Design decisions and bottlenecks

The reader preserves fixed-size chunk semantics for both POSIX `read()` and `mmap()`. The producer only hands workers chunks ending at token delimiters, which keeps tokenization correct across chunk boundaries. The bounded queue applies backpressure, and workers own private `WordIndex` instances so normal index updates need no shared-map lock. A single merge after worker joins is deterministic and avoids lock contention in the hot path.

The optional process mode is intentionally separated from the throughput path: its `fork()`/`exec()` boundary isolates index construction, but serializing a complete index over a pipe adds copying and latency. Persistence uses a temporary file, file `fsync()`, atomic `rename()`, and Linux directory `fsync()` to favor recoverability over write latency.

Likely bottlenecks, in order to validate with the scripts, are:

- The one producer reading and delimiter-aligning chunks at high worker counts.
- Token construction, lowercasing, and hash-map insertion for high-cardinality logs.
- Memory bandwidth and page faults, especially with `mmap()` on cold data.
- The final merge of worker-local maps when many unique words exist.
- Full-index pipe serialization in `--process-indexer` mode and synchronous durability work when `--index-dir` is enabled.

For comparable results, record the reader mode, worker count, buffer size, file size and token distribution, storage type, cache state (cold or warm), operating system, compiler, and CPU. Use several iterations; benchmark cold-cache I/O separately from warmed-cache CPU scaling.
