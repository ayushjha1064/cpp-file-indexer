#pragma once
#include "index.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

using namespace std;

class Query {
public:
    virtual ~Query() = default;
    virtual void run(const VersionStore& store) const = 0;
};

class WordCountQuery : public Query {
public:
    WordCountQuery(string version, string word);
    void run(const VersionStore& store) const override;

private:
    string version_;
    string word_;
};

class DiffQuery : public Query {
public:
    DiffQuery(string v1, string v2, string word);
    void run(const VersionStore& store) const override;

private:
    string v1_, v2_, word_;
};

class TopKQuery : public Query {
public:
    TopKQuery(string version, size_t k);
    void run(const VersionStore& store) const override;

private:
    string version_;
    size_t k_;
};

// Template utility (template requirement)
template <typename T, typename ScoreFn, typename TieFn>
vector<T> topKPartial(vector<T> items, size_t k, ScoreFn scoreFn, TieFn tieFn) {
    if (k > items.size()) k = items.size();
    if (k == 0) return {};

    partial_sort(items.begin(), items.begin() + k, items.end(),
                 [&](const T& a, const T& b) {
                     auto sa = scoreFn(a);
                     auto sb = scoreFn(b);
                     if (sa != sb) return sa > sb;
                     return tieFn(a, b);
                 });

    items.resize(k);
    return items;
}