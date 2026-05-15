#pragma once
#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

class BufferedFileReader {
public:
    explicit BufferedFileReader(size_t bufferBytes);

    void open(const string& path);
    size_t readChunk(); // returns bytes read; 0 => EOF

    const char* data() const noexcept;
    size_t capacity() const noexcept;

private:
    size_t bufSize_;
    vector<char> buffer_;
    ifstream in_;
};