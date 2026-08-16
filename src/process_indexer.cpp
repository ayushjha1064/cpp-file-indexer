#include "process_indexer.h"

#include <cerrno>
#include <cstdint>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

namespace {

constexpr uint64_t kProtocolMagic = 0x46494C4549445831ULL; // FILEIDX1
constexpr uint64_t kMaxEntries = 100000000;
constexpr uint64_t kMaxWordBytes = 16 * 1024 * 1024;

void closeNoThrow(int fileDescriptor) noexcept {
    if (fileDescriptor != -1) {
        ::close(fileDescriptor);
    }
}

void writeAll(int fileDescriptor, const void* data, size_t size) {
    const char* current = static_cast<const char*>(data);
    size_t remaining = size;
    while (remaining > 0) {
        const ssize_t bytesWritten = ::write(fileDescriptor, current, remaining);
        if (bytesWritten > 0) {
            current += bytesWritten;
            remaining -= static_cast<size_t>(bytesWritten);
            continue;
        }
        if (bytesWritten == -1 && errno == EINTR) {
            continue;
        }
        throw runtime_error("Failed to write index to pipe.");
    }
}

void readExact(int fileDescriptor, void* data, size_t size) {
    char* current = static_cast<char*>(data);
    size_t remaining = size;
    while (remaining > 0) {
        const ssize_t bytesRead = ::read(fileDescriptor, current, remaining);
        if (bytesRead > 0) {
            current += bytesRead;
            remaining -= static_cast<size_t>(bytesRead);
            continue;
        }
        if (bytesRead == -1 && errno == EINTR) {
            continue;
        }
        if (bytesRead == 0) {
            throw runtime_error("Indexer process ended before sending a complete index.");
        }
        throw runtime_error("Failed to read index from pipe.");
    }
}

void waitForChild(pid_t childPid) {
    int status = 0;
    pid_t waitResult;
    do {
        waitResult = ::waitpid(childPid, &status, 0);
    } while (waitResult == -1 && errno == EINTR);

    if (waitResult == -1) {
        throw runtime_error("Failed to wait for indexer process.");
    }
    if (WIFSIGNALED(status)) {
        throw runtime_error("Indexer process was terminated by signal " + to_string(WTERMSIG(status)) + ".");
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        throw runtime_error("Indexer process exited with status " + to_string(WEXITSTATUS(status)) + ".");
    }
}

} // namespace

void writeWordIndexToFd(int fileDescriptor, const WordIndex& index) {
    const vector<pair<string, long long>> items = index.allItems();
    const uint64_t entryCount = static_cast<uint64_t>(items.size());
    writeAll(fileDescriptor, &kProtocolMagic, sizeof(kProtocolMagic));
    writeAll(fileDescriptor, &entryCount, sizeof(entryCount));

    for (const auto& [word, count] : items) {
        if (count <= 0 || word.size() > kMaxWordBytes) {
            throw runtime_error("Cannot serialize invalid word-frequency entry.");
        }
        const uint64_t wordBytes = static_cast<uint64_t>(word.size());
        const int64_t frequency = static_cast<int64_t>(count);
        writeAll(fileDescriptor, &wordBytes, sizeof(wordBytes));
        writeAll(fileDescriptor, word.data(), word.size());
        writeAll(fileDescriptor, &frequency, sizeof(frequency));
    }
}

WordIndex readWordIndexFromFd(int fileDescriptor) {
    uint64_t magic = 0;
    uint64_t entryCount = 0;
    readExact(fileDescriptor, &magic, sizeof(magic));
    readExact(fileDescriptor, &entryCount, sizeof(entryCount));
    if (magic != kProtocolMagic || entryCount > kMaxEntries) {
        throw runtime_error("Indexer process sent an invalid index protocol.");
    }

    WordIndex index;
    for (uint64_t entry = 0; entry < entryCount; ++entry) {
        uint64_t wordBytes = 0;
        int64_t frequency = 0;
        readExact(fileDescriptor, &wordBytes, sizeof(wordBytes));
        if (wordBytes == 0 || wordBytes > kMaxWordBytes ||
            wordBytes > numeric_limits<size_t>::max()) {
            throw runtime_error("Indexer process sent an invalid word entry.");
        }

        string word(static_cast<size_t>(wordBytes), '\0');
        readExact(fileDescriptor, word.data(), word.size());
        readExact(fileDescriptor, &frequency, sizeof(frequency));
        if (frequency <= 0 || frequency > numeric_limits<long long>::max()) {
            throw runtime_error("Indexer process sent an invalid word frequency.");
        }
        index.addCount(word, static_cast<long long>(frequency));
    }
    return index;
}

WordIndex buildIndexInChildProcess(const string& executablePath,
                                   const string& filePath,
                                   size_t bufferBytes,
                                   ReaderMode readerMode,
                                   size_t workerCount) {
    int pipeFds[2] = {-1, -1};
    if (::pipe(pipeFds) == -1) {
        throw runtime_error("Failed to create indexer pipe.");
    }

    const pid_t childPid = ::fork();
    if (childPid == -1) {
        closeNoThrow(pipeFds[0]);
        closeNoThrow(pipeFds[1]);
        throw runtime_error("Failed to fork indexer process.");
    }

    if (childPid == 0) {
        closeNoThrow(pipeFds[0]);
        if (::dup2(pipeFds[1], STDOUT_FILENO) == -1) {
            _exit(127);
        }
        closeNoThrow(pipeFds[1]);

        const string bufferKilobytes = to_string(bufferBytes / 1024);
        const string workerCountText = to_string(workerCount);
        const char* readerModeText = readerMode == ReaderMode::MMap ? "mmap" : "read";
        ::execlp(executablePath.c_str(), executablePath.c_str(),
                 "--internal-export-index", "--file", filePath.c_str(),
                 "--buffer", bufferKilobytes.c_str(), "--reader", readerModeText,
                 "--workers", workerCountText.c_str(),
                 static_cast<char*>(nullptr));
        _exit(127);
    }

    closeNoThrow(pipeFds[1]);
    exception_ptr readError;
    WordIndex index;
    try {
        index = readWordIndexFromFd(pipeFds[0]);
    } catch (...) {
        readError = current_exception();
    }
    closeNoThrow(pipeFds[0]);
    waitForChild(childPid);

    if (readError != nullptr) {
        rethrow_exception(readError);
    }
    return index;
}
