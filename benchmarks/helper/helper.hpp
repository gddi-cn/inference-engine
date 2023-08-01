/**
 * @file helper.hpp
 * @author cc
 * @brief 
 * @version 0.1
 * @date 2022-10-10
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#pragma once
#include <array>
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <chrono>
#include <cstdio>
#include <exception>
#include <fcntl.h>
#include <iostream>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_set>

namespace gddi::benchmark::helper {
namespace fs = boost::filesystem;

inline bool find_find_path_impl(std::string &result, const fs::path &path_to_search,
                                const std::string &named_to_search,
                                std::unordered_set<std::string> &ignore_paths) {
    try {
        for (const auto &entry_ : fs::directory_iterator(path_to_search)) {
            if (fs::is_regular_file(entry_.status())) {
                if (entry_.path().filename().string() == named_to_search) {
                    result = entry_.path().string();
                    return true;
                }
            } else if (fs::is_directory(entry_.status())) {
                if (ignore_paths.count(entry_.path().string())) { continue; }
                ignore_paths.insert(entry_.path().string());
                if (find_find_path_impl(result, entry_.path(), named_to_search, ignore_paths)) {
                    return true;
                }
            }
        }

        // All sub-dir find finish. find parent.
        if (path_to_search.has_parent_path()) {
            auto parent = path_to_search.parent_path();
            std::string path_parent = parent.string();
            if (ignore_paths.count(path_parent) == 0) {
                ignore_paths.insert(path_parent);
                // spdlog::info("Search Parent: {}", path_parent);
                return find_find_path_impl(result, parent, named_to_search, ignore_paths);
            }
        }
    } catch (std::exception &e) { return false; }
    return false;
}

inline std::string find_file_path(const std::string &file_name) {
    std::string result;
    std::unordered_set<std::string> searched_dirs;
    if (find_find_path_impl(result, fs::current_path(), file_name, searched_dirs)) {
        spdlog::info("Found<{} dir>: {}", searched_dirs.size(), result);
    }

    return result;
}

template<typename DataType = char>
inline bool read_file(const std::string &file_name, std::vector<DataType> &data) noexcept {
    try {
        auto p = fs::path(file_name);
        if (fs::exists(p)) {
            auto size = fs::file_size(p);
            data.resize(size);

            auto f = std::fopen(file_name.c_str(), "rb");
            auto rd_size = std::fread(data.data(), sizeof(DataType), size / sizeof(DataType), f);
            std::fclose(f);
            if (rd_size == size) { return true; }
        }
        return false;
    } catch (std::exception &e) { return false; }
}

template<std::size_t N = 2>
class perf_timer {
public:
    typedef std::chrono::high_resolution_clock::time_point time_point;
    typedef std::function<void(const std::array<double, N - 1> &times)> metric_cb;

private:
    std::array<time_point, N> metrics_time_points_;
    std::array<double, N - 1> metrics_times_;
    std::size_t metric_index_ = 0;
    metric_cb metric_cb_;

public:
    perf_timer(const metric_cb &cb = nullptr) : metric_cb_(cb) {}
    void metric() {
        if (metric_index_ < N) {
            metrics_time_points_[metric_index_] = std::chrono::high_resolution_clock::now();
            metric_index_++;

            if (metric_index_ == N) {
                // Build Time Metrics
                for (std::size_t i = 0; i < metrics_times_.size(); i++) {
                    metrics_times_[i] = std::chrono::duration<double>(metrics_time_points_[i + 1]
                                                                      - metrics_time_points_[0])
                                            .count();
                }
                if (metric_cb_) { metric_cb_(metrics_times_); }
            }
        }
    }

    double time_last() const { return metrics_times_.back(); }
};

}// namespace gddi::benchmark::helper