//
// Created by cc on 2021/11/15.
//

#ifndef FFMPEG_WRAPPER_BENCHMARKS_BENCHMARK_UTILS_HPP_
#define FFMPEG_WRAPPER_BENCHMARKS_BENCHMARK_UTILS_HPP_
#include "blockingconcurrentqueue.h"
#include <memory>
#include <thread>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <locale>
#include <mutex>
#include <condition_variable>
#include <unordered_set>
#include <functional>
#ifdef WITH_CPPZMQ
#include <zmq.hpp>
#endif
#include "utils.hpp"

template<int Num_, int Pool_ = 2048, int PoolNumMax_ = 10>
struct MessageBytes {
    char data[Num_];
    MessageBytes() : data{} {}

    struct ManagedMem {
        std::mutex mutex;
        std::condition_variable condition_variable;
        std::unordered_set<void *> freed_list;
        int alloc_count;
        ManagedMem() : alloc_count(0) {}

        void *alloc(std::size_t count) {
            void *alloc_ptr = nullptr;
            std::unique_lock<std::mutex> lk(mutex);
            condition_variable.wait(lk, [this] {
                return (alloc_count < PoolNumMax_) || !freed_list.empty();
            });

            if (!freed_list.empty()) {
                auto iter = freed_list.begin();
                alloc_ptr = *iter;
                freed_list.erase(iter);
            } else {
                alloc_count++;
                alloc_ptr = ::operator new(count);
            }
            lk.unlock();
            condition_variable.notify_all();
            return alloc_ptr;
        }

        void free_ptr(void *ptr) {
            {
                std::lock_guard<std::mutex> lock_guard(mutex);
                freed_list.insert(ptr);
            }
            condition_variable.notify_all();
        }
    };

    static ManagedMem &get_cls() {
        static ManagedMem managed_mem;
        return managed_mem;
    }

    // static void operator delete(void *ptr) {
    //     if (Num_ > Pool_) {
    //         get_cls().free_ptr(ptr);
    //     } else {
    //         ::operator delete(ptr);
    //     }
    // }

    // static void *operator new(std::size_t count) {
    //     if (Num_ > Pool_) {
    //         return get_cls().alloc(count);
    //     } else {
    //         return ::operator new(count);
    //     }
    // }

};

struct ConsumerProducerTimeUse {
    int64_t consumer_time_used;
    int64_t producer_time_used;
};

std::thread metrics_run(const std::function<void()> &functor, int64_t &time_used) {
    return std::thread([&] {
        auto time_now_ = std::chrono::high_resolution_clock::now();

        functor();

        auto time_over_ = std::chrono::high_resolution_clock::now();
        time_used = std::chrono::duration_cast<std::chrono::microseconds>(time_over_ - time_now_).count();
    });
}

ConsumerProducerTimeUse benchmark_consumer_producer(
    const std::function<void()> &consumer_,
    const std::function<void()> &producer_) {
    ConsumerProducerTimeUse time_used{0};

    std::thread consumer = metrics_run(consumer_, time_used.consumer_time_used);
    std::thread producer = metrics_run(producer_, time_used.producer_time_used);

    producer.join();
    consumer.join();

    return time_used;
}

template<int MessageSize_, int TestCount_>
void blocking_concurrent_test() {
    typedef MessageBytes<MessageSize_> MessageSized;
    typedef std::shared_ptr<MessageSized> message1k_ptr;
    moodycamel::BlockingConcurrentQueue<message1k_ptr> queue_;
    int32_t message_test_total = TestCount_;

    std::cout << "\nbench for blocking, "
              << gddi::utils::human_readable_bytes(MessageSize_) << ", "
              << gddi::utils::human_readable_bytes(TestCount_) << " Sample\n";

    ConsumerProducerTimeUse time_used = benchmark_consumer_producer([&] {
        int32_t read_num_ = 0;
        message1k_ptr message_ptr;
        while (read_num_ + 10 < message_test_total) {
            queue_.wait_dequeue(message_ptr);
            read_num_++;
        }
    }, [&] {
        int32_t generate_num_ = 0;
        while (generate_num_ < message_test_total) {
            // queue_.enqueue(std::shared_ptr<MessageSized>(new MessageSized));
            queue_.enqueue(std::shared_ptr<MessageSized>(std::make_shared<MessageSized>()));
            generate_num_++;
        }
    });

    std::cout.imbue(std::locale(""));
    auto oldflags = std::cout.flags();
    std::cout << std::showpoint
              << "producer total: " << message_test_total << std::endl
              << "producer time: " << time_used.producer_time_used
              << "us, " << message_test_total * 1000LL * 1000LL / time_used.producer_time_used << " msg/s"
              << std::endl
              << "consumer time: " << time_used.consumer_time_used
              << "us, " << message_test_total * 1000LL * 1000LL / time_used.consumer_time_used << " msg/s"
              << std::endl;
    std::cout.flags(oldflags);
    std::cout << "POOL size: " << MessageSized::get_cls().freed_list.size() << std::endl;
}

#endif //FFMPEG_WRAPPER_BENCHMARKS_BENCHMARK_UTILS_HPP_
