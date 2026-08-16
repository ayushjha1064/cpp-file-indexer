#include "persistent_index.h"

#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

int main() {
    char directoryTemplate[] = "/tmp/file-indexer-persistent.XXXXXX";
    char* directory = ::mkdtemp(directoryTemplate);
    if (directory == nullptr) {
        throw runtime_error("Failed to create persistent-index test directory.");
    }

    const string version = "release/2026.08";
    const string path = persistentIndexPath(directory, version);
    try {
        WordIndex first;
        first.addCount("alpha", 2);
        first.addCount("beta", 1);
        saveVersionAtomically(directory, version, first);

        struct stat status {};
        assert(::stat(path.c_str(), &status) == 0);
        assert(S_ISREG(status.st_mode));
        assert(loadVersionFromDisk(directory, version).getCount("alpha") == 2);

        WordIndex replacement;
        replacement.addCount("alpha", 5);
        replacement.addCount("gamma", 3);
        saveVersionAtomically(directory, version, replacement);

        WordIndex restored = loadVersionFromDisk(directory, version);
        assert(restored.getCount("alpha") == 5);
        assert(restored.getCount("beta") == 0);
        assert(restored.getCount("gamma") == 3);
        assert(restored.uniqueWords() == 2);
    } catch (...) {
        ::unlink(path.c_str());
        ::rmdir(directory);
        throw;
    }

    assert(::unlink(path.c_str()) == 0);
    assert(::rmdir(directory) == 0);
}
