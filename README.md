# C++ File Indexer

A C++ file indexing system that processes large text/log files using fixed-size buffers and supports efficient word-frequency analysis.

## Features

- Processes large files using fixed-size buffers
- Maintains constant memory overhead
- Supports incremental indexing
- Provides word-frequency queries
- Supports Top-K and Difference queries
- Modular design using separate reader, tokenizer, index, query, and CLI components

## Project Structure

```text
.
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
│   ├── queries.cpp
│   └── queries.h
├── data/
└── build/
