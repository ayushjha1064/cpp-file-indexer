#include "tokenizer.h"
#include <cctype>

using namespace std;

static inline bool isAlnum(unsigned char c) {
    return isalnum(c) != 0;
}

static inline char toLowerChar(unsigned char c) {
    return static_cast<char>(tolower(c));
}

vector<string> Tokenizer::consume(const char* data, size_t n) {
    vector<string> out;
    size_t i = 0;

    // Extend carry from previous chunk
    if (!carry_.empty()) {
        while (i < n && isAlnum(static_cast<unsigned char>(data[i]))) {
            carry_.push_back(toLowerChar(static_cast<unsigned char>(data[i])));
            ++i;
        }
        if(i == n){
            return out; // still partial
        }
        out.push_back(carry_);
        carry_.clear();
    }

    while (i < n) {
        while (i < n && !isAlnum(static_cast<unsigned char>(data[i]))) ++i;
        if (i >= n) break;

        string tok;
        while (i < n && isAlnum(static_cast<unsigned char>(data[i]))) {
            tok.push_back(toLowerChar(static_cast<unsigned char>(data[i])));
            ++i;
        }

        if (i == n) {
            carry_ = std::move(tok); // possible partial
        } else {
            out.push_back(std::move(tok));
        }
    }

    return out;
}

optional<string> Tokenizer::flush() {
    if (carry_.empty()) return nullopt;
    string t = std::move(carry_);
    carry_.clear();
    return t;
}

bool Tokenizer::hasCarry() const noexcept {
    return !carry_.empty();
}