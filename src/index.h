#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std;

class WordIndex {
public:
    void add(const string& word);

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
                              size_t bufferBytes);

    bool hasVersion(const string& versionName) const noexcept;
    const WordIndex& get(const string& versionName) const;

private:
    unordered_map<string, WordIndex> versions_;
};