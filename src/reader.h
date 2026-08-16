#pragma once
#include <cstddef>
#include <string>
#include <vector>

using namespace std;

enum class ReaderMode {
    Read,
    MMap,
};

class BufferedFileReader {
public:
    explicit BufferedFileReader(size_t bufferBytes, ReaderMode mode = ReaderMode::Read);
    ~BufferedFileReader();

    BufferedFileReader(const BufferedFileReader&) = delete;
    BufferedFileReader& operator=(const BufferedFileReader&) = delete;

    void open(const string& path);
    size_t readChunk(); // returns bytes read; 0 => EOF

    const char* data() const noexcept;
    size_t capacity() const noexcept;

private:
    void closeCurrent() noexcept;

    size_t bufSize_;
    vector<char> buffer_;
    ReaderMode mode_;
    int fd_ = -1;
    void* mapping_ = nullptr;
    size_t mappingSize_ = 0;
    size_t mappingOffset_ = 0;
    size_t dataOffset_ = 0;
};
