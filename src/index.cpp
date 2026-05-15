#include "index.h"
#include "reader.h"
#include "tokenizer.h"
#include <stdexcept>

using namespace std;

void WordIndex::add(const string& word) {
    if (word.empty()) return;
    ++freq_[word];
}

long long WordIndex::getCount(const string& word) const {
    auto it = freq_.find(word);
    return it == freq_.end() ? 0LL : it->second;
}

long long WordIndex::getCount(const string& word, long long defaultVal) const {
    auto it = freq_.find(word);
    return it == freq_.end() ? defaultVal : it->second;
}

size_t WordIndex::uniqueWords() const noexcept {
    return freq_.size();
}

vector<pair<string, long long>> WordIndex::allItems() const {
    vector<pair<string, long long>> items;
    items.reserve(freq_.size());
    for (const auto& kv : freq_) items.push_back(kv);
    return items;
}

void VersionStore::buildVersionFromFile(const string& versionName,
                                        const string& filePath,
                                        size_t bufferBytes) {
    if (versionName.empty()) {
        throw invalid_argument("Version name cannot be empty.");
    }
    if (versions_.count(versionName)) {
        throw runtime_error("Duplicate version name: " + versionName);
    }

    BufferedFileReader reader(bufferBytes);
    reader.open(filePath);

    Tokenizer tok;
    WordIndex idx;

    while (true) {
        size_t n = reader.readChunk();
        if (n == 0) break;
        auto tokens = tok.consume(reader.data(), n);
        for (const auto& w : tokens) idx.add(w);
    }
    if (auto last = tok.flush()) idx.add(*last);

    versions_.emplace(versionName, std::move(idx));
}

bool VersionStore::hasVersion(const string& versionName) const noexcept {
    return versions_.find(versionName) != versions_.end();
}

const WordIndex& VersionStore::get(const string& versionName) const {
    auto it = versions_.find(versionName);
    if (it == versions_.end()) {
        throw runtime_error("Unknown version: " + versionName);
    }
    return it->second;
}