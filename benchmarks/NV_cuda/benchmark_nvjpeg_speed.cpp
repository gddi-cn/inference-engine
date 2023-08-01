/**
 * @file benchmark_nvjpeg_speed.cpp
 * @author cc
 * @brief 
 * @version 0.1
 * @date 2022-10-11
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "NV_cuda/nvJpegDecoderPriv.hpp"
#include "common_basic/thread_worker.hpp"
#include "fmt/format.h"
#include "helper/helper.hpp"
#include "indicators/block_progress_bar.hpp"
#include "indicators/cursor_control.hpp"
#include "nvJpegDecoder.hpp"
#include "spdlog/spdlog.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

void bench_decode(double time_max, const std::shared_ptr<std::vector<char>> &data, int batch_size = 1) {
    std::size_t total_run_n = 0;
    double total_time_used = 0;
    auto time_begin = std::chrono::high_resolution_clock::now();
    gddi::experiments::NvJpegDecoder decoder;
    gddi::experiments::FileData fileData;
    for (int i = 0; i < batch_size; i++) { fileData.push_back(data); }

    double time_cpu = 0;
    double time_gpu = 0;
    while (true) {
        auto time_cur = std::chrono::high_resolution_clock::now();
        auto time_elapsed = std::chrono::duration<double>(time_cur - time_begin).count();
        if (time_elapsed > time_max) {
            total_time_used = time_elapsed;
            break;
        }

        decoder.priv.process_images(fileData);
        time_cpu += decoder.priv.last_decode_cpu_time;
        time_gpu += decoder.priv.last_decode_gpu_time;
        total_run_n += fileData.size();
    }

    fmt::print("NvJpeg Deocder\n");
    fmt::print("\t   Batch Size: {}\n", batch_size);
    fmt::print("\t   Bench Time: {:.3f}s, real used: {:.3f}\n", time_max, total_time_used);
    fmt::print("\t             : CPU: {:.3f}s, GPU: {:.3f}\n", time_cpu, time_gpu);
    fmt::print("\tTotal Decoded: {}, FPS: {:.3f}\n", total_run_n, total_run_n / total_time_used);
}

void bench_async(int max_n, const std::shared_ptr<std::vector<char>> &data) {
    gddi::experiments::NvJpegDecoder decoder;

    std::atomic_int fin_count = 0;

    using namespace indicators;
    indicators::show_console_cursor(false);
    indicators::BlockProgressBar bar{option::BarWidth{50}, option::PrefixText{"Benchmark Progress: "},
                                     option::MaxProgress{max_n}};

    auto time_0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < max_n; i++) decoder.enqueue(data, [&](bool, const std::string &) { fin_count++; });

    while (fin_count != max_n) {
        bar.set_progress(fin_count);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto time_1 = std::chrono::high_resolution_clock::now();

    bar.set_progress(max_n);
    bar.mark_as_completed();
    indicators::show_console_cursor(true);
    auto time_used = std::chrono::duration<double>(time_1 - time_0).count();
    spdlog::info("bench {} used: {:.3f}s, {:.1f} fps", max_n, time_used, max_n / time_used);
}

void bench_multi(int max_n, size_t w_n, const std::shared_ptr<std::vector<char>> &data) {
    gddi::experiments::NvJpegDecoder2 decoder(w_n);

    std::atomic_int fin_count = 0;

    using namespace indicators;
    indicators::show_console_cursor(false);
    indicators::BlockProgressBar bar{option::BarWidth{50}, option::PrefixText{"Benchmark Progress: "},
                                     option::MaxProgress{max_n}};

    auto time_0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < max_n; i++) decoder.enqueue(data, [&](bool, const std::string &) { fin_count++; });

    while (fin_count != max_n) {
        bar.set_progress(fin_count);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto time_1 = std::chrono::high_resolution_clock::now();

    bar.set_progress(max_n);
    bar.mark_as_completed();
    indicators::show_console_cursor(true);
    auto time_used = std::chrono::duration<double>(time_1 - time_0).count();
    spdlog::info("bench {} used: {:.3f}s, {:.1f} fps, Worker: {}", max_n, time_used, max_n / time_used, w_n);
}

void bench_simple(const std::shared_ptr<std::vector<char>> &data) {
    gddi::experiments::NvJpegDeocderSimple decoder;
    decoder.test_process(data);
}

void bench_multi_simple(int max_n, size_t w_n, const std::shared_ptr<std::vector<char>> &data) {
    std::atomic_int fin_count = 0;

    gddi::QueuedWorker<std::vector<char>> queued_w;
    std::vector<std::shared_ptr<gddi::experiments::NvJpegDeocderSimple>> workers;

    for (size_t i = 0; i < w_n; i++) {
        auto w = std::make_shared<gddi::experiments::NvJpegDeocderSimple>();
        workers.push_back(w);
        queued_w.add_worker(fmt::format("NV-DEC-{}", i), [=, &fin_count](const std::shared_ptr<std::vector<char>> &m) {
            w->test_process(m);
            fin_count++;
        });
    }

    using namespace indicators;
    indicators::show_console_cursor(false);
    indicators::BlockProgressBar bar{option::BarWidth{50}, option::PrefixText{"Benchmark Progress: "},
                                     option::MaxProgress{max_n}};

    auto time_0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < max_n; i++) queued_w.enqueue(data);

    while (fin_count != max_n) {
        bar.set_progress(fin_count);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto time_1 = std::chrono::high_resolution_clock::now();

    bar.set_progress(max_n);
    bar.mark_as_completed();
    indicators::show_console_cursor(true);
    auto time_used = std::chrono::duration<double>(time_1 - time_0).count();
    spdlog::info("bench {} used: {:.3f}s, {:.1f} fps, Worker: {}", max_n, time_used, max_n / time_used, w_n);
}

int main(int argc, char *argv[]) {
    auto &&sample_jpg_path = gddi::benchmark::helper::find_file_path("jiwei_1920x1080.jpeg");

    // std::vector<char> data;
    auto data = std::make_shared<std::vector<char>>();

    if (gddi::benchmark::helper::read_file(sample_jpg_path, *data)) {
        spdlog::info("File Size: {}", data->size());

        // bench_decode(2, data, 1);
        bench_simple(data);
        // bench_multi_simple(2000, 1, data);
        // bench_multi_simple(2000, 2, data);
        // bench_multi_simple(4000, 4, data);
        for (int i = 1; i < std::thread::hardware_concurrency() + 1; i++) bench_multi_simple(2000 * i, i, data);

        // bench_decode(4, data, 2);
        // bench_async(1000, data);
        // bench_async(2000, data);
        // bench_multi(2000, 1, data);
        // bench_multi(2000, 2, data);
        // bench_multi(8000, 3, data);
        // bench_multi(8000, 4, data);
        // bench_multi(8000, 5, data);
        // bench_multi(8000, 6, data);
        // bench_decode(2, data, 4);
        // bench_decode(2, data, 8);
    }

    return 0;
}