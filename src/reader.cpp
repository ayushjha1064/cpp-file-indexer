#include "reader.h"

#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

BufferedFileReader::BufferedFileReader(size_t bufferBytes, ReaderMode mode)
    : bufSize_(bufferBytes),
      buffer_(mode == ReaderMode::Read ? bufferBytes : 0, '\0'),
      mode_(mode) {
    constexpr size_t MIN_BUF = 256 * 1024;
    constexpr size_t MAX_BUF = 1024 * 1024;
    if (bufSize_ < MIN_BUF || bufSize_ > MAX_BUF) {
        throw invalid_argument("Buffer size must be between 256KB (262144) and 1024KB (1048576).");
    }
}

BufferedFileReader::~BufferedFileReader() {
    closeCurrent();
}

void BufferedFileReader::closeCurrent() noexcept {
    if (mapping_ != nullptr) {
        ::munmap(mapping_, mappingSize_);
        mapping_ = nullptr;
    }
    mappingSize_ = 0;
    mappingOffset_ = 0;
    dataOffset_ = 0;

    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }
}

void BufferedFileReader::open(const string& path) {
    const int newFd = ::open(path.c_str(), O_RDONLY);
    if (newFd == -1) {
        throw runtime_error("Failed to open file: " + path);
    }

    void* newMapping = nullptr;
    size_t newMappingSize = 0;
    if (mode_ == ReaderMode::MMap) {
        struct stat fileStatus {};
        if (::fstat(newFd, &fileStatus) == -1) {
            ::close(newFd);
            throw runtime_error("Failed to stat file: " + path);
        }
        if (!S_ISREG(fileStatus.st_mode)) {
            ::close(newFd);
            throw runtime_error("Memory-mapped mode requires a regular file: " + path);
        }
        if (fileStatus.st_size < 0 ||
            static_cast<unsigned long long>(fileStatus.st_size) > numeric_limits<size_t>::max()) {
            ::close(newFd);
            throw runtime_error("File is too large to memory-map: " + path);
        }

        newMappingSize = static_cast<size_t>(fileStatus.st_size);
        if (newMappingSize > 0) {
            newMapping = ::mmap(nullptr, newMappingSize, PROT_READ, MAP_PRIVATE, newFd, 0);
            if (newMapping == MAP_FAILED) {
                ::close(newFd);
                throw runtime_error("Failed to memory-map file: " + path);
            }
        }
    }

    closeCurrent();
    fd_ = newFd;
    mapping_ = newMapping;
    mappingSize_ = newMappingSize;
}

size_t BufferedFileReader::readChunk() {
    if (fd_ == -1) {
        throw runtime_error("BufferedFileReader: file not opened.");
    }

    if (mode_ == ReaderMode::MMap) {
        if (mappingOffset_ == mappingSize_) {
            return 0;
        }
        const size_t remaining = mappingSize_ - mappingOffset_;
        const size_t chunkSize = remaining < bufSize_ ? remaining : bufSize_;
        dataOffset_ = mappingOffset_;
        mappingOffset_ += chunkSize;
        return chunkSize;
    }

    ssize_t bytesRead;
    do {
        bytesRead = ::read(fd_, buffer_.data(), bufSize_);
    } while (bytesRead == -1 && errno == EINTR);

    if (bytesRead == -1) {
        throw runtime_error("BufferedFileReader: I/O error.");
    }

    return static_cast<size_t>(bytesRead);
}

const char* BufferedFileReader::data() const noexcept {
    if (mode_ == ReaderMode::MMap) {
        if (mapping_ == nullptr) {
            return nullptr;
        }
        return static_cast<const char*>(mapping_) + dataOffset_;
    }
    return buffer_.data();
}
size_t BufferedFileReader::capacity() const noexcept { return bufSize_; }
