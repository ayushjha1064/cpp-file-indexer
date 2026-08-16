#include "index.h"

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <unistd.h>

using namespace std;

namespace {

void writeAll(int fileDescriptor, const string& data) {
    size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t bytesWritten = ::write(fileDescriptor,
                                             data.data() + offset,
                                             data.size() - offset);
        if (bytesWritten > 0) {
            offset += static_cast<size_t>(bytesWritten);
            continue;
        }
        if (bytesWritten == -1 && errno == EINTR) {
            continue;
        }
        throw runtime_error("Failed to write pipeline test input.");
    }
}

} // namespace

int main() {
    char path[] = "/tmp/file-indexer-pipeline.XXXXXX";
    const int fileDescriptor = ::mkstemp(path);
    if (fileDescriptor == -1) {
        throw runtime_error("Failed to create pipeline test input.");
    }
    bool fileOpen = true;

    try {
        string data(262142, ' ');
        data += "CrossingWord ";
        for (size_t i = 0; i < 20000; ++i) {
            data += "alpha beta gamma ";
        }
        writeAll(fileDescriptor, data);
        fileOpen = false;
        if (::close(fileDescriptor) == -1) {
            throw runtime_error("Failed to close pipeline test input.");
        }

        VersionStore singleWorker;
        singleWorker.buildVersionFromFile("single", path, 256 * 1024, ReaderMode::Read, 1);

        VersionStore parallelRead;
        parallelRead.buildVersionFromFile("parallel-read", path, 256 * 1024, ReaderMode::Read, 4);

        VersionStore parallelMMap;
        parallelMMap.buildVersionFromFile("parallel-mmap", path, 256 * 1024, ReaderMode::MMap, 4);

        for (const WordIndex* index : {&singleWorker.get("single"),
                                       &parallelRead.get("parallel-read"),
                                       &parallelMMap.get("parallel-mmap")}) {
            assert(index->getCount("crossingword") == 1);
            assert(index->getCount("alpha") == 20000);
            assert(index->getCount("beta") == 20000);
            assert(index->getCount("gamma") == 20000);
        }
    } catch (...) {
        if (fileOpen) {
            ::close(fileDescriptor);
        }
        ::unlink(path);
        throw;
    }

    assert(::unlink(path) == 0);
}
