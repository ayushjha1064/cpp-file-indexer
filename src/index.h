#pragma once
#include "reader.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std;

class WordIndex {
public:
    void add(const string& word);
    void addCount(const string& word, long long count);
    void mergeFrom(const WordIndex& other);

    // Overloading requirement
    long long getCount(const string& word) const;
    long long getCount(const string& word, long long defaultVal) const;

    size_t uniqueWords() const noexcept;
    vector<pair<string, long long>> allItems() const;

private:
    unordered_map<string, long long> freq_;
};

class VersionStore {
public:
    void buildVersionFromFile(const string& versionName,
                              const string& filePath,
                              size_t bufferBytes,
                              ReaderMode readerMode = ReaderMode::Read,
                              size_t workerCount = 1);
    void addVersion(const string& versionName, WordIndex index);

    bool hasVersion(const string& versionName) const noexcept;
    const WordIndex& get(const string& versionName) const;

private:
    unordered_map<string, WordIndex> versions_;
};
