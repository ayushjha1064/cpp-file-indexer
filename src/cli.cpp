#include "cli.h"
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>

using namespace std;

static optional<string> getFlag(int argc, char** argv, const string& key) {
    for (int i = 1; i + 1 < argc; i++) {
        if (argv[i] == key) return string(argv[i + 1]);
    }
    return nullopt;
}

static bool hasFlag(int argc, char** argv, const string& key) {
    for (int i = 1; i < argc; i++) {
        if (argv[i] == key) return true;
    }
    return false;
}

static void validateNoUnknownFlags(int argc, char** argv) {
    static const unordered_set<string> known = {
        "--help",
        "--query",
        "--buffer",
        "--file", "--version", "--word",
        "--file1", "--version1", "--file2", "--version2",
        // strict spec uses --top; keep --k as a backward-compatible alias
        "--top", "--k"
    };

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (!arg.empty() && arg[0] == '-') {
            if (known.find(arg) == known.end()) {
                throw runtime_error("Unknown flag: " + arg);
            }
        }
    }
}

Args parseArgs(int argc, char** argv) {
    Args a;

    if (hasFlag(argc, argv, "--help")) {
        a.help = true;
        return a;
    }

    validateNoUnknownFlags(argc, argv);

    if (auto q = getFlag(argc, argv, "--query")) a.queryType = *q;

    // Normalize query type to strict-spec names
    if (a.queryType == "topk") a.queryType = "top";

    // Buffer is in KB (default 256)
    if (auto b = getFlag(argc, argv, "--buffer")) {
        a.bufferKB = static_cast<size_t>(stoull(*b));
    }

    if (a.bufferKB < 256 || a.bufferKB > 1024) {
        throw runtime_error("Buffer size (KB) must be between 256 and 1024.");
    }

    if (auto f = getFlag(argc, argv, "--file")) a.file = *f;
    if (auto v = getFlag(argc, argv, "--version")) a.version = *v;
    if (auto w = getFlag(argc, argv, "--word")) a.word = *w;

    if (auto f1 = getFlag(argc, argv, "--file1")) a.file1 = *f1;
    if (auto v1 = getFlag(argc, argv, "--version1")) a.version1 = *v1;
    if (auto f2 = getFlag(argc, argv, "--file2")) a.file2 = *f2;
    if (auto v2 = getFlag(argc, argv, "--version2")) a.version2 = *v2;

    // strict spec: --top <k> (alias: --k <k>)
    if (auto t = getFlag(argc, argv, "--top")) a.k = static_cast<size_t>(stoull(*t));
    else if (auto k = getFlag(argc, argv, "--k")) a.k = static_cast<size_t>(stoull(*k));

    if (a.queryType.empty()) {
        throw runtime_error("Missing --query (use --help for usage).");
    }

    if (a.queryType == "word") {
        if (a.file.empty() || a.version.empty() || a.word.empty()) {
            throw runtime_error("word query requires --file --version --word");
        }
    } else if (a.queryType == "diff") {
        if (a.file1.empty() || a.version1.empty() || a.file2.empty() || a.version2.empty() || a.word.empty()) {
            throw runtime_error("diff query requires --file1 --version1 --file2 --version2 --word");
        }
        if (a.version1 == a.version2) {
            throw runtime_error("diff query requires distinct --version1 and --version2 names");
        }
    } else if (a.queryType == "top") {
        if (a.file.empty() || a.version.empty() || a.k == 0) {
            throw runtime_error("top query requires --file --version --top (k>0)");
        }
    } else {
        throw runtime_error("Unknown query type: " + a.queryType + " (expected: word|diff|top)");
    }

    return a;
}

void printUsage(const char* progName) {
    string p = progName ? progName : "analyzer";
    cout
        << "Usage:\n"
        << "  " << p << " --help\n\n"
        << "  Word Count Query:\n"
        << "    " << p << " --query word --file <path> --version <name> --buffer <KB> --word <w>\n\n"
        << "  Top-K Query:\n"
        << "    " << p << " --query top --file <path> --version <name> --buffer <KB> --top <K>\n\n"
        << "  Difference Query:\n"
        << "    " << p << " --query diff --file1 <path> --version1 <name> --file2 <path> --version2 <name> --buffer <KB> --word <w>\n\n"
        << "Notes:\n"
        << "  - Buffer must be between 256 and 1024 KB.\n"
        << "  - Tokenization is contiguous alphanumeric sequences, case-insensitive.\n";
}