#include "cli.h"
#include "index.h"
#include "process_indexer.h"
#include "persistent_index.h"
#include "queries.h"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <unistd.h>

using namespace std;
using Clock = chrono::high_resolution_clock;

int main(int argc, char** argv) {
    try {
        Args a = parseArgs(argc, argv);

        if (a.help) {
            printUsage(argv[0]);
            return 0;
        }

        size_t bufferBytes = a.bufferKB * 1024;
        ReaderMode readerMode = a.readerMode == "mmap" ? ReaderMode::MMap : ReaderMode::Read;

        if (a.internalExportIndex) {
            VersionStore childStore;
            childStore.buildVersionFromFile("internal", a.file, bufferBytes, readerMode, a.workerCount);
            writeWordIndexToFd(STDOUT_FILENO, childStore.get("internal"));
            return 0;
        }

        auto start = Clock::now();

        VersionStore store;
        unique_ptr<Query> query;
        const auto buildVersion = [&](const string& versionName, const string& filePath) {
            if (a.loadIndex) {
                store.addVersion(versionName, loadVersionFromDisk(a.indexDirectory, versionName));
                return;
            }
            if (a.processIndexer) {
                store.addVersion(versionName,
                                 buildIndexInChildProcess(argv[0], filePath, bufferBytes,
                                                          readerMode, a.workerCount));
            } else {
                store.buildVersionFromFile(versionName, filePath, bufferBytes, readerMode, a.workerCount);
            }
            if (!a.indexDirectory.empty()) {
                saveVersionAtomically(a.indexDirectory, versionName, store.get(versionName));
            }
        };

        if (a.queryType == "word") {
            buildVersion(a.version, a.file);
            query = make_unique<WordCountQuery>(a.version, a.word);

            // Strict output format
            cout << "Version: " << a.version << "\n";
            cout << "Count: ";
            query->run(store);

            auto end = Clock::now();
            double seconds = chrono::duration<double>(end - start).count();

            cout << "Buffer Size (KB): " << (bufferBytes / 1024) << "\n";
            cout << "Execution Time (s): " << fixed << setprecision(5) << seconds << "\n";
            return 0;
        }

        if (a.queryType == "top") {
            buildVersion(a.version, a.file);
            query = make_unique<TopKQuery>(a.version, a.k);

            // Strict output format
            cout << "Top-" << a.k << " words in version " << a.version << ":\n";
            query->run(store);

            auto end = Clock::now();
            double seconds = chrono::duration<double>(end - start).count();

            cout << "Buffer Size (KB): " << (bufferBytes / 1024) << "\n";
            cout << "Execution Time (s): " << fixed << setprecision(5) << seconds << "\n";
            return 0;
        }

        if (a.queryType == "diff") {
            buildVersion(a.version1, a.file1);
            buildVersion(a.version2, a.file2);
            query = make_unique<DiffQuery>(a.version1, a.version2, a.word);

            // Strict output format
            cout << "Difference (" << a.version2 << " - " << a.version1 << "): ";
            query->run(store);

            auto end = Clock::now();
            double seconds = chrono::duration<double>(end - start).count();

            cout << "Buffer Size (KB): " << (bufferBytes / 1024) << "\n";
            cout << "Execution Time (s): " << fixed << setprecision(5) << seconds << "\n";
            return 0;
        }

        throw runtime_error("Unsupported query type.");

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        cerr << "Use --help for usage.\n";
        return 1;
    }
}
