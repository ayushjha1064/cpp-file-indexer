#include "process_indexer.h"

#include <cassert>
#include <stdexcept>
#include <unistd.h>

using namespace std;

int main() {
    int pipeFds[2] = {-1, -1};
    assert(::pipe(pipeFds) == 0);

    WordIndex source;
    source.addCount("alpha", 4);
    source.addCount("beta", 2);
    writeWordIndexToFd(pipeFds[1], source);
    assert(::close(pipeFds[1]) == 0);

    WordIndex restored = readWordIndexFromFd(pipeFds[0]);
    assert(::close(pipeFds[0]) == 0);
    assert(restored.getCount("alpha") == 4);
    assert(restored.getCount("beta") == 2);
    assert(restored.uniqueWords() == 2);

    bool rejectedInvalidCount = false;
    try {
        source.addCount("invalid", -1);
    } catch (const invalid_argument&) {
        rejectedInvalidCount = true;
    }
    assert(rejectedInvalidCount);
}
