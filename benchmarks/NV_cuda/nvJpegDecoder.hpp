/**
 * @file nvJpegDecoder.hpp
 * @author cc
 * @brief a clone refined version from https://github.com/NVIDIA/CUDALibrarySamples
 * @version 0.1
 * @date 2022-10-11
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#pragma once
#include "blockingconcurrentqueue.h"
#include "common_basic/thread_dbg_utils.hpp"
#include "nvJpegDecoderPriv.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace gddi::experiments {

class NvJpegDecoder {

public:
    NvJpegDecoder() {
        decoder_loop_ = std::thread([=] {
            ImageObject batch_decode[8];
            std::atomic_uint32_t max_queued = 0;

            gddi::thread_utils::set_cur_thread_name("Work");

            while (true) {
                FileData file_data;
                auto batch_real = queued_images_.wait_dequeue_bulk(batch_decode, 8);
                // 1. Check if break required
                for (size_t i = 0; i < batch_real; i++) {
                    if (batch_decode[i].image_data) {
                        file_data.push_back(batch_decode[i].image_data);
                    } else {
                        // Quit Thread
                        return;
                    }
                }

                // 2. Put to GPU Queue
                if (priv.process_images(file_data)) {
                    for (size_t i = 0; i < batch_real; i++) {
                        if (batch_decode[i].image_cb) { batch_decode[i].image_cb(true, ""); }
                    }
                }
            }
        });
    }

    ~NvJpegDecoder() {
        queued_images_.enqueue({nullptr, nullptr});
        if (decoder_loop_.joinable()) { decoder_loop_.join(); }
    }

    struct ImageObject {
        std::shared_ptr<std::vector<char>> image_data;
        std::function<void(bool, const std::string &what)> image_cb;
    };

    void enqueue(const std::vector<char> &image_data,
                 const std::function<void(bool, const std::string &what)> &image_cb) {
        auto im_data = std::make_shared<std::vector<char>>(image_data.begin(), image_data.end());
        queued_images_.enqueue({im_data, image_cb});
    }

    void enqueue(const std::shared_ptr<std::vector<char>> &image_data,
                 const std::function<void(bool, const std::string &what)> &image_cb) {
        queued_images_.enqueue({image_data, image_cb});
    }

public:
    NvJpegDecoderInstance priv;
    moodycamel::BlockingConcurrentQueue<ImageObject> queued_images_;
    std::thread decoder_loop_;
};

class NvJpegDecoder2 {
public:
    NvJpegDecoder2(size_t dec_instance_count = 2) {
        quit_workers_ = 0;
        for (size_t i = 0; i < dec_instance_count; i++) { add_worker(); }
    }

    ~NvJpegDecoder2() {
        quit_flag_ = true;
        for (size_t i = 0; i < 8 * workers_.size(); i++) { queued_images_.enqueue({nullptr, nullptr}); }

        std::unique_lock<std::mutex> l(quit_m_);
        quit_cond_.wait(l, [=]() { return quit_workers_ >= workers_.size(); });
        for (auto &w : workers_) w.join();
    }

public:
    void enqueue(const std::vector<char> &image_data,
                 const std::function<void(bool, const std::string &what)> &image_cb) {
        auto im_data = std::make_shared<std::vector<char>>(image_data.begin(), image_data.end());
        queued_images_.enqueue({im_data, image_cb});
    }

    void enqueue(const std::shared_ptr<std::vector<char>> &image_data,
                 const std::function<void(bool, const std::string &what)> &image_cb) {
        queued_images_.enqueue({image_data, image_cb});
    }

private:
    void add_worker() {
        auto instance = std::make_shared<NvJpegDecoderInstance>();
        instances_.push_back(instance);
        workers_.emplace_back(std::thread([=] {
            ImageObject batch_decode[8];
            gddi::thread_utils::set_cur_thread_name("Work");

            auto quit_action = [&] {
                // Quit Thread
                quit_workers_++;
                quit_cond_.notify_all();
            };

            while (true) {
                FileData file_data;
                // 1. Dequeue As More As 8(Batch)
                auto batch_real = queued_images_.wait_dequeue_bulk_timed(batch_decode, 8, 1000);
                if (batch_real == 0) {
                    if (quit_flag_) {
                        quit_action();
                        return;
                    }
                    // Next Wait
                    continue;
                }

                // 2. Check if break required
                for (size_t i = 0; i < batch_real; i++) {
                    if (batch_decode[i].image_data) {
                        file_data.push_back(batch_decode[i].image_data);
                    } else if (quit_flag_) {
                        quit_action();
                        return;
                    }
                }

                // 3. Put to GPU Queue
                if (instance->process_images(file_data)) {
                    for (size_t i = 0; i < batch_real; i++) {
                        if (batch_decode[i].image_cb) { batch_decode[i].image_cb(true, ""); }
                    }
                } else {
                    for (size_t i = 0; i < batch_real; i++) {
                        if (batch_decode[i].image_cb) { batch_decode[i].image_cb(false, "Image Not Supported"); }
                    }
                }
            }
        }));
    }

private:
    struct ImageObject {
        std::shared_ptr<std::vector<char>> image_data;
        std::function<void(bool, const std::string &what)> image_cb;
    };

    moodycamel::BlockingConcurrentQueue<ImageObject> queued_images_;
    std::vector<std::shared_ptr<NvJpegDecoderInstance>> instances_;
    std::vector<std::thread> workers_;

    std::atomic_int32_t quit_workers_;
    std::atomic_bool quit_flag_ = false;
    std::mutex quit_m_;
    std::condition_variable quit_cond_;
};

}// namespace gddi::experiments