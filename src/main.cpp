#include "cli.h"
#include "index.h"
#include "queries.h"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>

using namespace std;
using Clock = chrono::high_resolution_clock;

int main(int argc, char** argv) {
    try {
        Args a = parseArgs(argc, argv);

        if (a.help) {
            printUsage(argv[0]);
            return 0;
        }

        auto start = Clock::now();

        VersionStore store;
        unique_ptr<Query> query;
        size_t bufferBytes = a.bufferKB * 1024;

        if (a.queryType == "word") {
            store.buildVersionFromFile(a.version, a.file, bufferBytes);
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
            store.buildVersionFromFile(a.version, a.file, bufferBytes);
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
            store.buildVersionFromFile(a.version1, a.file1, bufferBytes);
            store.buildVersionFromFile(a.version2, a.file2, bufferBytes);
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