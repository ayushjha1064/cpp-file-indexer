#include "queries.h"
#include <cctype>
#include <iostream>
#include <stdexcept>

using namespace std;

static inline string toLower(string s) {
    for (char& ch : s) ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    return s;
}

static inline bool isValidWordQueryInput(const string& w) {
    return !w.empty();
}

WordCountQuery::WordCountQuery(string version, string word)
    : version_(std::move(version)), word_(toLower(std::move(word))) {
    if (version_.empty()) throw invalid_argument("WordCountQuery: version cannot be empty.");
    if (!isValidWordQueryInput(word_)) throw invalid_argument("WordCountQuery: word cannot be empty.");
}

void WordCountQuery::run(const VersionStore& store) const {
    const auto& idx = store.get(version_);
    cout << idx.getCount(word_) << "\n";
}

DiffQuery::DiffQuery(string v1, string v2, string word)
    : v1_(std::move(v1)), v2_(std::move(v2)), word_(toLower(std::move(word))) {
    if (v1_.empty() || v2_.empty()) throw invalid_argument("DiffQuery: version names cannot be empty.");
    if (!isValidWordQueryInput(word_)) throw invalid_argument("DiffQuery: word cannot be empty.");
}

void DiffQuery::run(const VersionStore& store) const {
    const auto& a = store.get(v1_);
    const auto& b = store.get(v2_);
    // Strict spec expects Difference (v2 - v1)
    long long diff = b.getCount(word_) - a.getCount(word_);
    cout << diff << "\n";
}

TopKQuery::TopKQuery(string version, size_t k)
    : version_(std::move(version)), k_(k) {
    if (version_.empty()) throw invalid_argument("TopKQuery: version cannot be empty.");
    if (k_ == 0) throw invalid_argument("TopKQuery: k must be > 0.");
}

void TopKQuery::run(const VersionStore& store) const {
    const auto& idx = store.get(version_);
    auto items = idx.allItems();

    if (items.empty()) return;

    auto top = topKPartial(
        std::move(items),
        k_,
        [](const auto& p) { return p.second; },
        [](const auto& a, const auto& b) { return a.first < b.first; }
    );

    for(const auto& p : top){
        cout<<p.first<<" "<<p.second<<"\n";
    }
}