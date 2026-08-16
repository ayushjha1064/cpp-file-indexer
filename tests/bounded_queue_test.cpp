#include "bounded_queue.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std;

int main() {
    BoundedBlockingQueue<int> queue(1);
    queue.push(1);

    promise<void> producerStarted;
    future<void> producerStartedFuture = producerStarted.get_future();
    future<void> producer = async(launch::async, [&queue, &producerStarted] {
        producerStarted.set_value();
        queue.push(2);
    });

    producerStartedFuture.wait();
    assert(producer.wait_for(chrono::milliseconds(50)) == future_status::timeout);
    assert(queue.pop().value() == 1);
    producer.get();
    assert(queue.pop().value() == 2);

    promise<void> consumerStarted;
    future<void> consumerStartedFuture = consumerStarted.get_future();
    future<optional<int>> consumer = async(launch::async, [&queue, &consumerStarted] {
        consumerStarted.set_value();
        return queue.pop();
    });

    consumerStartedFuture.wait();
    assert(consumer.wait_for(chrono::milliseconds(50)) == future_status::timeout);
    queue.close();
    assert(!consumer.get().has_value());
    assert(!queue.pop().has_value());

    bool rejectedClosedPush = false;
    try {
        queue.push(3);
    } catch (const logic_error&) {
        rejectedClosedPush = true;
    }
    assert(rejectedClosedPush);

    bool rejectedZeroCapacity = false;
    try {
        BoundedBlockingQueue<int> invalidQueue(0);
    } catch (const invalid_argument&) {
        rejectedZeroCapacity = true;
    }
    assert(rejectedZeroCapacity);

    BoundedBlockingQueue<int> multiProducerQueue(4);
    constexpr size_t producerCount = 3;
    constexpr size_t consumerCount = 2;
    constexpr size_t itemsPerProducer = 200;
    atomic<size_t> consumed{0};
    atomic<long long> sum{0};
    vector<thread> consumers;
    for (size_t i = 0; i < consumerCount; ++i) {
        consumers.emplace_back([&multiProducerQueue, &consumed, &sum] {
            while (optional<int> item = multiProducerQueue.pop()) {
                sum.fetch_add(*item);
                consumed.fetch_add(1);
            }
        });
    }

    vector<thread> producers;
    for (size_t i = 0; i < producerCount; ++i) {
        producers.emplace_back([&multiProducerQueue] {
            for (size_t item = 0; item < itemsPerProducer; ++item) {
                multiProducerQueue.push(1);
            }
        });
    }
    for (thread& producerThread : producers) {
        producerThread.join();
    }
    multiProducerQueue.close();
    for (thread& consumerThread : consumers) {
        consumerThread.join();
    }

    const size_t expectedItemCount = producerCount * itemsPerProducer;
    assert(consumed == expectedItemCount);
    assert(sum == static_cast<long long>(expectedItemCount));
}
