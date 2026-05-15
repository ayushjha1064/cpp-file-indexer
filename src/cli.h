#pragma once
#include <cstddef>
#include <string>

using namespace std;

struct Args {
    // Strict spec: "word" | "diff" | "top"
    // ("topk" is accepted as an alias and normalized to "top".)
    string queryType;
    size_t bufferKB = 256; // default 256KB

    // word/top
    string file, version, word;

    // diff
    string file1, version1, file2, version2;

    // top
    size_t k = 0;

    bool help = false;
};

Args parseArgs(int argc, char** argv);
void printUsage(const char* progName);