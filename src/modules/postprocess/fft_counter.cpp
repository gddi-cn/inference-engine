#include "fft_counter.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <opencv2/core.hpp>
#include <opencv2/core/types.hpp>
#include <ratio>
#include <utility>
#include <vector>

namespace gddi {

FFTCounter::FFTCounter(std::map<int, PosePoint> points, int sliding_window)
    : pose_points_(std::move(points)), sliding_window_(sliding_window) {}

int FFTCounter::update(const int track_id, const int64_t frame_idx,
                       const std::vector<nodes::PoseKeyPoint> &key_points) {
    cache_key_points_[track_id].emplace_back(frame_idx, key_points);

    if (last_range_len_.count(track_id) == 0) {
        last_range_len_.insert(std::make_pair(track_id, 0));
        erase_count_.insert(std::make_pair(track_id, 0));
    }

    std::map<int, int> count;
    for (int i = 0; i < 17; i++) {
        if (pose_points_.at(i).weight == 0) { continue; }

        std::vector<double> data;
        for (const auto &item : cache_key_points_.at(track_id)) {
            switch (pose_points_.at(i).orientation) {
                case Orientation::kHorizontal: data.emplace_back(item.second[i].x); break;
                case Orientation::kVertical: data.emplace_back(item.second[i].y); break;
                case Orientation::kScore: data.emplace_back(item.second[i].prob); break;
                default:;// xy求范数
            }
        }

        int num_action = fft_count(data);
        if (num_action != -1) { ++count[num_action + erase_count_.at(track_id)]; }
    }

    if (count.empty()) {
        range_[track_id].push_back(-1);
    } else {
        range_[track_id].push_back(
            std::max_element(count.begin(), count.end(),
                             [](const std::pair<int, int> &value1, const std::pair<int, int> &value2) {
                                 return value1.second < value2.second;
                             })
                ->first);
    }

    return range_[track_id].back();
}

int FFTCounter::fft_count(const std::vector<double> &data) {
    std::vector<double> fft;
    cv::dft(data, fft);

    auto len = data.size();
    for (auto &item : fft) { item /= len; }

    int freq = 1;
    double max_value = 0;
    for (int i = 1; i < len; i += 2) {
        double value = fft[i] * fft[i];
        if (i + 1 < len) { value += fft[i + 1] * fft[i + 1]; }
        if (value > max_value) {
            max_value = value;
            freq = i / 2 + 1;
        }
    }

    if (freq != 1 && freq < len / 2) { return freq; }

    return -1;
}

void FFTCounter::get_range(const int track_id, std::map<int64_t, std::vector<nodes::PoseKeyPoint>> &action_key_point) {
    int len = range_[track_id].size();
    // for (int prev = 0, cur = 0; cur < len;) {
    //     if (range_[track_id][prev] == range_[track_id][cur]) {
    //         ++cur;
    //     } else {
    //         prev = cur - 1;
    //         auto prev_value = range_[track_id][prev];
    //         if (std::find(range_[track_id].begin() + cur, range_[track_id].begin() + cur + 3, prev_value)
    //             != range_[track_id].begin() + cur + 3) {
    //             while (range_[track_id][cur] != prev_value) { range_[track_id][cur++] = prev_value; }
    //         } else {
    //             ++cur;
    //         }
    //     }
    // }

    // for (auto &value : range_[track_id]) { printf("%d ", value); }
    // printf("\n==================================\n");

    int start_idx = 0;
    int end_idx = 0;
    int smallest_idx = floor(floor((range_[track_id].size() + erase_count_.at(track_id))
                                   / *std::max_element(range_[track_id].begin(), range_[track_id].end()))
                             / 3);
    std::vector<std::pair<int, int>> result;
    while (end_idx < len) {
        if (range_[track_id][start_idx] == range_[track_id][end_idx]) {
            end_idx++;
        } else {
            if (end_idx - start_idx > smallest_idx) { result.emplace_back(start_idx, end_idx); }
            start_idx = end_idx;
            ++end_idx;
        }
    }
    if (end_idx - start_idx > smallest_idx) { result.emplace_back(start_idx, end_idx); }

    if (result.size() - last_range_len_.at(track_id) >= 2) {
        // 处理区间合并 || 区间长度 < 3
        if (last_range_len_.at(track_id) > result.size()) {
            last_range_len_.at(track_id) = result.size() - 1;
            printf("========== resize range len\n");
            return;
        }

        for (int i = result[last_range_len_.at(track_id)].first;
             i < result[last_range_len_.at(track_id)].second && i < cache_key_points_.at(track_id).size(); i++) {
            // TODO: 边界问题
            action_key_point.insert(cache_key_points_.at(track_id)[i]);
        }
        // spdlog::debug("# track_id: {}, {} -- {}", track_id, result[last_range_len_.at(track_id)].first,
        //               result[last_range_len_.at(track_id)].second,
        //               result[last_range_len_.at(track_id)].second - result[last_range_len_.at(track_id)].first);
        ++last_range_len_.at(track_id);
    }

    // int index = 1;
    // for (const auto &item : result) {
    //     spdlog::debug("# {}, id: {}, {} -- {}, frames: {}", index++, track_id, item.first, item.second,
    //                   item.second - item.first);
    // }
    // spdlog::debug("=========================================");

    if (sliding_window_ > 0) {
        if (cache_key_points_.at(track_id).size() > sliding_window_) {
            int erase_len = result.front().second - result.front().first;
            cache_key_points_.at(track_id).erase(cache_key_points_.at(track_id).begin(),
                                                 cache_key_points_.at(track_id).begin() + erase_len);
            range_.at(track_id).erase(range_.at(track_id).begin(), range_.at(track_id).begin() + erase_len);
            --last_range_len_.at(track_id);
            ++erase_count_.at(track_id);
        }
    }
}

void normalize_keypoints(const std::vector<nodes::PoseKeyPoint> &key_points,
                         std::vector<nodes::PoseKeyPoint> &normalized) {
    nodes::PoseKeyPoint mean_value{0, 0, 0, 0};
    int len = key_points.size();
    for (int i = 0; i < len; i++) {
        mean_value.x += key_points[i].x;
        mean_value.y += key_points[i].y;
    }

    mean_value.x /= len;
    mean_value.y /= len;

    nodes::PoseKeyPoint std_value{0, 0, 0, 0};
    for (int i = 0; i < len; i++) {
        std_value.x += (key_points[i].x - mean_value.x) * (key_points[i].x - mean_value.x);
        std_value.y += (key_points[i].y - mean_value.y) * (key_points[i].y - mean_value.y);
    }

    std_value.x = std::sqrt(std_value.x / len);
    std_value.y = std::sqrt(std_value.y / len);

    for (int i = 0; i < len; i++) {
        normalized.emplace_back(nodes::PoseKeyPoint{.number = key_points[i].number,
                                                    .x = (key_points[i].x - mean_value.x) / std_value.x,
                                                    .y = (key_points[i].y - mean_value.y) / std_value.y,
                                                    .prob = 0});
    }
}

double distance_keypoints(const std::vector<nodes::PoseKeyPoint> &a, const std::vector<nodes::PoseKeyPoint> &b,
                          const std::map<int, PosePoint> &ff_points) {
    std::vector<nodes::PoseKeyPoint> a_normalized;
    std::vector<nodes::PoseKeyPoint> b_normalized;
    normalize_keypoints(a, a_normalized);
    normalize_keypoints(b, b_normalized);

    int len = a_normalized.size();
    std::vector<double> distance;
    for (int i = 0; i < len; i++) {
        distance.emplace_back(
            std::sqrt((b_normalized[i].x - a_normalized[i].x) * (b_normalized[i].x - a_normalized[i].x)
                      + (b_normalized[i].y - a_normalized[i].y) * (b_normalized[i].y - a_normalized[i].y)));
    }

    for (int i = 0; i < ff_points.size(); i++) { distance[i] *= ff_points.at(i).weight; }

    for (int i = 1; i < len; i++) { distance[0] += distance[i]; }

    return distance[0] / len;
}

void distance_keypoints(const std::vector<nodes::PoseKeyPoint> &a, const std::vector<nodes::PoseKeyPoint> &b,
                        const std::map<int, PosePoint> &ff_points, std::vector<double> &distance) {
    std::vector<nodes::PoseKeyPoint> a_normalized;
    std::vector<nodes::PoseKeyPoint> b_normalized;
    normalize_keypoints(a, a_normalized);
    normalize_keypoints(b, b_normalized);

    int len = a_normalized.size();
    for (int i = 0; i < len; i++) {
        distance.emplace_back(
            std::sqrt((b_normalized[i].x - a_normalized[i].x) * (b_normalized[i].x - a_normalized[i].x)
                      + (b_normalized[i].y - a_normalized[i].y) * (b_normalized[i].y - a_normalized[i].y)));
    }

    for (int i = 0; i < ff_points.size(); i++) { distance[i] *= ff_points.at(i).weight; }
}

ActionPair::ActionPair(std::map<int, PosePoint> points) : pose_points_(points) {}

double ActionPair::pair_action(const std::map<int, std::vector<nodes::PoseKeyPoint>> &current_range,
                               const std::vector<std::vector<nodes::PoseKeyPoint>> &standard_range,
                               std::map<std::pair<int, int>, std::vector<double>> &pairs) {
    int rows = current_range.size();
    int cols = standard_range.size();

    double dp[rows][cols];
    int min_map[rows][cols];
    memset(min_map, 0, rows * cols * sizeof(int));

    for (int i = 0; i < rows; i++) {
        int min_index = 0;
        for (int j = 0; j < cols; j++) {
            auto dis =
                distance_keypoints(current_range.at(current_range.begin()->first + i), standard_range[j], pose_points_)
                + std::abs(i / rows - j / cols);
            if (i == 0) {
                dp[i][j] = dis;
            } else {
                if (j == 0) {
                    dp[i][j] = dp[i - 1][j] + dis;
                } else {
                    dp[i][j] = dp[i - 1][min_map[i - 1][j - 1]] + dis;
                }
            }

            if (j > 0) {
                if (dp[i][j] >= dp[i][min_map[i][j - 1]]) {
                    min_map[i][j] = min_map[i][j - 1];
                } else {
                    min_map[i][j] = j;
                }
            }
        }
    }

    int j = cols - 1;
    for (int i = rows - 1; i >= 0; i--) {
        j = min_map[i][j];

        std::vector<double> distance;
        distance_keypoints(current_range.at(current_range.begin()->first + i), standard_range[j], pose_points_,
                           distance);
        pairs.insert(std::make_pair(std::make_pair(i, j), std::move(distance)));
    }

    return dp[rows - 1][min_map[rows - 1][cols - 1]];
}

}// namespace gddi