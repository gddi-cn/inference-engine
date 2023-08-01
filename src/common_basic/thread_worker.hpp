#pragma once
/**
 * @file thread_worker.hpp
 * @author cc.pxy
 * @brief 
 * @version 0.1
 * @date 2022-10-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "common_basic/thread_dbg_utils.hpp"
#include <algorithm>
#include <atomic>
#include <blockingconcurrentqueue.h>
#include <chrono>
#include <cstddef>
#include <fmt/format.h>
#include <functional>
#include <spdlog/spdlog.h>
#include <thread>
#include <vector>

namespace gddi {
class WorkerPool {
public:
    WorkerPool(const std::string &name, std::size_t worker_num = 4) : name_(name) {
        run_(worker_num);
    }
    ~WorkerPool() { force_stop(); }

    void enqueue(std::function<void()> task) { queued_works_.enqueue(std::move(task)); }

    void quit_wait_empty() {
        while (queued_works_.size_approx() != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        force_stop();
    }

    std::size_t size_approx() const { return queued_works_.size_approx(); }

private:
    void force_stop() {
        request_stop_ = true;
        queued_works_.enqueue(nullptr);
        while (stopped_num_ != (int)workers_.size()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        for (auto &w : workers_) {
            if (w.joinable()) { w.join(); }
        }
    }

private:
    void run_(std::size_t n) {
        for (std::size_t i = 0; i < n; i++) { build_worker(i); }
    }

    void build_worker(std::size_t wid) {
        workers_.emplace_back([=] {
            spdlog::info("Setup thread: {}-{}", name_, wid);
            std::function<void()> task_to_run;
            thread_utils::set_cur_thread_name(fmt::format("{} {:2}", name_, wid));
            while (true) {
                queued_works_.wait_dequeue(task_to_run);

                if (task_to_run) { task_to_run(); }

                if (request_stop_) {
                    stopped_num_++;
                    queued_works_.enqueue(nullptr);
                    break;
                }
            }
        });
    }

private:
    moodycamel::BlockingConcurrentQueue<std::function<void()>> queued_works_;
    std::vector<std::thread> workers_;
    std::string name_;
    std::atomic_bool request_stop_ = false;
    std::atomic_int32_t stopped_num_ = 0;
};

/**
 * @brief 面向数据的并行工作处理队列
 * 
 * @tparam DataType 
 */
template<class DataType>
class QueuedWorker {
public:
    QueuedWorker() = default;
    ~QueuedWorker() { force_stop(); }

    typedef std::shared_ptr<DataType> SharedDataPtr;
    typedef std::function<void(const SharedDataPtr &)> DataHandler;

    void enqueue(const SharedDataPtr &d) { queued_data_.enqueue(d); }
    void add_worker(const std::string &name,
                    const std::function<void(const SharedDataPtr &data_ptr)> &func) {
        workers_.emplace_back([=] {
            spdlog::info("Setup Data Worker: {}", name);
            gddi::thread_utils::set_cur_thread_name(name);
            SharedDataPtr data;

            while (true) {
                queued_data_.wait_dequeue(data);

                if (data) { func(data); }

                if (request_stop_) {
                    stopped_num_++;
                    queued_data_.enqueue(nullptr);
                    break;
                }
            }
        });
    }

    std::size_t size_approx() const { return queued_data_.size_approx(); }
    void quit_wait_empty() {
        while (queued_data_.size_approx() != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        force_stop();
    }

private:
    void force_stop() {
        request_stop_ = true;
        queued_data_.enqueue(nullptr);
        while (stopped_num_ != (int)workers_.size()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        for (auto &w : workers_) {
            if (w.joinable()) { w.join(); }
        }
    }

private:
    moodycamel::BlockingConcurrentQueue<SharedDataPtr> queued_data_;
    std::vector<DataHandler> data_handlers_;
    std::vector<std::thread> workers_;
    std::atomic_bool request_stop_ = false;
    std::atomic_int32_t stopped_num_ = 0;
};

}// namespace gddi