#include "reader.h"
#include <stdexcept>
#include<iostream>
using namespace std;

BufferedFileReader::BufferedFileReader(size_t bufferBytes)
    : bufSize_(bufferBytes), buffer_(bufferBytes, '\0') {
    constexpr size_t MIN_BUF = 256 * 1024;
    constexpr size_t MAX_BUF = 1024 * 1024;
    if (bufSize_ < MIN_BUF || bufSize_ > MAX_BUF) {
        throw invalid_argument("Buffer size must be between 256KB (262144) and 1024KB (1048576).");
    }
}

void BufferedFileReader::open(const string& path) {
    cerr << "DEBUG opening: [" << path << "]\n";
    in_.open(path, ios::binary);
    cerr << "DEBUG is_open=" << in_.is_open() << " good=" << static_cast<bool>(in_) << "\n";
    if (!in_) throw runtime_error("Failed to open file: " + path);
}

size_t BufferedFileReader::readChunk() {
    // Only treat "not opened" as error
    if (!in_.is_open()) {
        throw runtime_error("BufferedFileReader: file not opened.");
    }

    // If previous read hit EOF, clear eofbit so that I can attempt another read and get gcount=0 cleanly.
    if (in_.eof()) {
        in_.clear();
    } else if (in_.fail() && !in_.bad()) {
        // failbit without badbit: usually recoverable, but here it's unexpected
        // (optional: you can just clear and continue)
        in_.clear();
    } else if (in_.bad()) {
        throw runtime_error("BufferedFileReader: I/O error (badbit set).");
    }

    in_.read(buffer_.data(), static_cast<streamsize>(bufSize_));
    return static_cast<size_t>(in_.gcount());
}

const char* BufferedFileReader::data() const noexcept { return buffer_.data(); }
size_t BufferedFileReader::capacity() const noexcept { return bufSize_; }