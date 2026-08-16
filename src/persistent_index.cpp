#include "persistent_index.h"

#include "process_indexer.h"

#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace std;

namespace {

void closeOrThrow(int fileDescriptor, const string& description) {
    if (::close(fileDescriptor) == -1) {
        throw runtime_error("Failed to close " + description + ".");
    }
}

void closeNoThrow(int fileDescriptor) noexcept {
    if (fileDescriptor != -1) {
        ::close(fileDescriptor);
    }
}

void fsyncOrThrow(int fileDescriptor, const string& description) {
    int result;
    do {
        result = ::fsync(fileDescriptor);
    } while (result == -1 && errno == EINTR);
    if (result == -1) {
        throw runtime_error("Failed to fsync " + description + ".");
    }
}

void ensureDirectory(const string& directory) {
    if (directory.empty()) {
        throw invalid_argument("Index directory cannot be empty.");
    }
    if (::mkdir(directory.c_str(), 0755) == -1 && errno != EEXIST) {
        throw runtime_error("Failed to create index directory: " + directory);
    }

    struct stat status {};
    if (::stat(directory.c_str(), &status) == -1 || !S_ISDIR(status.st_mode)) {
        throw runtime_error("Index path is not a directory: " + directory);
    }
}

void fsyncDirectory(const string& directory) {
    const int directoryFd = ::open(directory.c_str(), O_RDONLY);
    if (directoryFd == -1) {
        throw runtime_error("Failed to open index directory for fsync: " + directory);
    }

    int result;
    do {
        result = ::fsync(directoryFd);
    } while (result == -1 && errno == EINTR);
    if (result == -1) {
#ifndef __linux__
        const int fsyncError = errno;
#endif
        closeNoThrow(directoryFd);
#ifdef __linux__
        throw runtime_error("Failed to fsync index directory.");
#else
        if (fsyncError == EINVAL) {
            return;
        }
        throw runtime_error("Failed to fsync index directory.");
#endif
    }
    closeOrThrow(directoryFd, "index directory");
}

string encodedVersionName(const string& versionName) {
    if (versionName.empty()) {
        throw invalid_argument("Version name cannot be empty.");
    }

    static constexpr char hex[] = "0123456789abcdef";
    string encoded;
    encoded.reserve(versionName.size() * 2);
    for (unsigned char character : versionName) {
        encoded.push_back(hex[character >> 4]);
        encoded.push_back(hex[character & 0x0f]);
    }
    return encoded;
}

string joinPath(const string& directory, const string& fileName) {
    if (!directory.empty() && directory.back() == '/') {
        return directory + fileName;
    }
    return directory + "/" + fileName;
}

} // namespace

string persistentIndexPath(const string& directory, const string& versionName) {
    return joinPath(directory, "version-" + encodedVersionName(versionName) + ".idx");
}

void saveVersionAtomically(const string& directory,
                           const string& versionName,
                           const WordIndex& index) {
    ensureDirectory(directory);
    const string finalPath = persistentIndexPath(directory, versionName);
    string temporaryPath = joinPath(directory, ".file-indexer-" + encodedVersionName(versionName) + ".XXXXXX");
    vector<char> temporaryPathBuffer(temporaryPath.begin(), temporaryPath.end());
    temporaryPathBuffer.push_back('\0');

    int temporaryFd = ::mkstemp(temporaryPathBuffer.data());
    if (temporaryFd == -1) {
        throw runtime_error("Failed to create temporary index file.");
    }
    temporaryPath.assign(temporaryPathBuffer.data());
    bool renamed = false;

    try {
        if (::fchmod(temporaryFd, 0644) == -1) {
            throw runtime_error("Failed to set index file permissions.");
        }
        writeWordIndexToFd(temporaryFd, index);
        fsyncOrThrow(temporaryFd, "temporary index file");
        closeOrThrow(temporaryFd, "temporary index file");
        temporaryFd = -1;

        if (::rename(temporaryPath.c_str(), finalPath.c_str()) == -1) {
            throw runtime_error("Failed to atomically replace index file.");
        }
        renamed = true;
        fsyncDirectory(directory);
    } catch (...) {
        closeNoThrow(temporaryFd);
        if (!renamed) {
            ::unlink(temporaryPath.c_str());
        }
        throw;
    }
}

WordIndex loadVersionFromDisk(const string& directory, const string& versionName) {
    const string path = persistentIndexPath(directory, versionName);
    const int fileDescriptor = ::open(path.c_str(), O_RDONLY);
    if (fileDescriptor == -1) {
        throw runtime_error("Failed to open persistent index: " + path);
    }

    try {
        WordIndex index = readWordIndexFromFd(fileDescriptor);
        closeOrThrow(fileDescriptor, "persistent index file");
        return index;
    } catch (...) {
        closeNoThrow(fileDescriptor);
        throw;
    }
}
