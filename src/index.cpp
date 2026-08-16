#include "index.h"
#include "bounded_queue.h"
#include "tokenizer.h"

#include <cctype>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>

using namespace std;

namespace {

constexpr size_t kQueuedChunkCapacity = 8;

struct FileChunk {
    string data;
};

bool isAlphanumeric(char character) {
    return isalnum(static_cast<unsigned char>(character)) != 0;
}

void indexChunk(const FileChunk& chunk, WordIndex& index) {
    Tokenizer tokenizer;
    for (const string& word : tokenizer.consume(chunk.data.data(), chunk.data.size())) {
        index.add(word);
    }
    if (optional<string> lastWord = tokenizer.flush()) {
        index.add(*lastWord);
    }
}

void enqueueCompleteChunks(BufferedFileReader& reader,
                           BoundedBlockingQueue<FileChunk>& queue) {
    string pending;

    while (true) {
        const size_t bytesRead = reader.readChunk();
        if (bytesRead == 0) {
            break;
        }
        pending.append(reader.data(), bytesRead);

        size_t splitOffset = pending.size();
        while (splitOffset > 0 && isAlphanumeric(pending[splitOffset - 1])) {
            --splitOffset;
        }
        if (splitOffset == 0) {
            continue;
        }

        queue.push(FileChunk{pending.substr(0, splitOffset)});
        pending.erase(0, splitOffset);
    }

    if (!pending.empty()) {
        queue.push(FileChunk{std::move(pending)});
    }
}

void joinThreads(vector<thread>& threads) noexcept {
    for (thread& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

} // namespace

void WordIndex::add(const string& word) {
    if (word.empty()) return;
    ++freq_[word];
}

void WordIndex::addCount(const string& word, long long count) {
    if (count < 0) {
        throw invalid_argument("Word frequency cannot be negative.");
    }
    if (word.empty() || count == 0) return;
    freq_[word] += count;
}

void WordIndex::mergeFrom(const WordIndex& other) {
    for (const auto& [word, count] : other.freq_) {
        freq_[word] += count;
    }
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
                                        size_t bufferBytes,
                                        ReaderMode readerMode,
                                        size_t workerCount) {
    if (versionName.empty()) {
        throw invalid_argument("Version name cannot be empty.");
    }
    if (versions_.count(versionName)) {
        throw runtime_error("Duplicate version name: " + versionName);
    }
    if (workerCount == 0) {
        throw invalid_argument("Worker count must be greater than zero.");
    }

    BufferedFileReader reader(bufferBytes, readerMode);
    reader.open(filePath);

    WordIndex idx;
    BoundedBlockingQueue<FileChunk> workQueue(kQueuedChunkCapacity);
    vector<WordIndex> partialIndexes(workerCount);
    vector<thread> workers;
    workers.reserve(workerCount);
    exception_ptr workerError;
    mutex workerErrorMutex;

    try {
        for (size_t worker = 0; worker < workerCount; ++worker) {
            workers.emplace_back([&workQueue, &partialIndexes, &workerError, &workerErrorMutex, worker] {
                try {
                    while (optional<FileChunk> chunk = workQueue.pop()) {
                        indexChunk(*chunk, partialIndexes[worker]);
                    }
                } catch (...) {
                    {
                        lock_guard<mutex> lock(workerErrorMutex);
                        if (workerError == nullptr) {
                            workerError = current_exception();
                        }
                    }
                    workQueue.close();
                }
            });
        }
    } catch (...) {
        workQueue.close();
        joinThreads(workers);
        throw;
    }

    exception_ptr producerError;
    try {
        enqueueCompleteChunks(reader, workQueue);
    } catch (...) {
        producerError = current_exception();
    }
    workQueue.close();
    joinThreads(workers);

    if (workerError != nullptr) {
        rethrow_exception(workerError);
    }
    if (producerError != nullptr) {
        rethrow_exception(producerError);
    }

    for (const WordIndex& partialIndex : partialIndexes) {
        idx.mergeFrom(partialIndex);
    }

    addVersion(versionName, std::move(idx));
}

void VersionStore::addVersion(const string& versionName, WordIndex index) {
    if (versionName.empty()) {
        throw invalid_argument("Version name cannot be empty.");
    }
    if (versions_.count(versionName)) {
        throw runtime_error("Duplicate version name: " + versionName);
    }
    versions_.emplace(versionName, std::move(index));
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
