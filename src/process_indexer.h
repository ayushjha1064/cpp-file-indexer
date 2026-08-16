#pragma once

#include "index.h"

#include <cstddef>
#include <string>

WordIndex buildIndexInChildProcess(const std::string& executablePath,
                                   const std::string& filePath,
                                   size_t bufferBytes,
                                   ReaderMode readerMode,
                                   size_t workerCount);

void writeWordIndexToFd(int fileDescriptor, const WordIndex& index);
WordIndex readWordIndexFromFd(int fileDescriptor);
