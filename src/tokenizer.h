#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

using namespace std;

class Tokenizer {
public:
    vector<string> consume(const char* data, size_t n);
    optional<string> flush();
    bool hasCarry() const noexcept;

private:
    string carry_;
};