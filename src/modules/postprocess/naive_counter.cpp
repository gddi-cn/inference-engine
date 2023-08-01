#include "naive_counter.h"
#include "spdlog/spdlog.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace gddi {

static float square_of_distance(const nodes::PoseKeyPoint &point1, const nodes::PoseKeyPoint &point2) {
    return std::pow((point2.x - point1.x), 2) + std::pow((point2.y - point1.y), 2);
}

static float calculate_angle(const cv::Vec4f &line1, const cv::Vec4f &line2) {
    auto m1 = (line1[3] - line1[1]) / (line1[2] - line1[0]);
    auto m2 = (line2[3] - line2[1]) / (line2[2] - line2[0]);
    return std::atan((m2 - m1) / (1 + m1 * m2)) * 180 / 3.14159265;
}

class NaiveCounterPrivate {
public:
    NaiveCounterPrivate(const size_t count) {}
    ~NaiveCounterPrivate() {}

    bool update_one_point_x(const int track_id, const int index, const nodes::PoseKeyPoint &cur_key_point,
                            const float threshold) {
        auto &cur_status = status_[track_id][index];
        auto &prev_key_point = prev_key_points_[track_id][index];

        if (__glibc_unlikely(cur_status.size() == 0)) {
            cur_status.emplace_back(0);
        } else if ((__glibc_unlikely(cur_status.size() == 1))) {
            cur_status.emplace_back(cur_status.front());
        } else if (std::abs(cur_key_point.x - prev_key_point.x) >= threshold) {
            cur_status.emplace_back(cur_key_point.x > prev_key_point.x);
        }

        prev_key_point = cur_key_point;

        return remove_noise(cur_status);
    }

    bool update_one_point_y(const int track_id, const int index, const nodes::PoseKeyPoint &cur_key_point,
                            const float threshold) {
        auto &cur_status = status_[track_id][index];
        auto &prev_key_point = prev_key_points_[track_id][index];

        if (__glibc_unlikely(cur_status.size() == 0)) {
            cur_status.emplace_back(0);
        } else if ((__glibc_unlikely(cur_status.size() == 1))) {
            cur_status.emplace_back(cur_status.front());
        } else if (std::abs(cur_key_point.y - prev_key_point.y) >= threshold) {
            cur_status.emplace_back(cur_key_point.y > prev_key_point.y);
        }

        prev_key_point = cur_key_point;

        return remove_noise(cur_status);
    }

    bool update_distance(const int track_id, const int index, const nodes::PoseKeyPoint &key_point1,
                         const nodes::PoseKeyPoint &key_point2, const float threshold) {
        auto &cur_status = status_[track_id][index];
        auto &prev_distance = prev_distances_[track_id][index];

        auto distance = square_of_distance(key_point1, key_point2);

        // 两点距离变化
        if (__glibc_unlikely(cur_status.size() == 0)) {
            cur_status.emplace_back(1);
        } else if ((__glibc_unlikely(cur_status.size() == 1))) {
            cur_status.emplace_back(cur_status.front());
        } else if (std::abs(distance - prev_distance) >= threshold * threshold) {
            cur_status.emplace_back(distance > prev_distance);
        }

        prev_distance = distance;

        return remove_noise(cur_status);
    }

    bool update_angle(const int track_id, const int index, const nodes::PoseKeyPoint &key_point1,
                      const nodes::PoseKeyPoint &key_point2, const nodes::PoseKeyPoint &key_point3,
                      const float threshold) {
        auto &cur_status = status_[track_id][index];
        auto &prev_angle = prev_angles_[track_id][index];

        auto line1 = cv::Vec4f(key_point1.x, key_point1.y, key_point2.x, key_point2.y);
        auto line2 = cv::Vec4f(key_point2.x, key_point2.y, key_point3.x, key_point3.y);
        auto angle = calculate_angle(line1, line2);

        // 三个点角度变化
        if (__glibc_unlikely(cur_status.size() <= 1)) {
            cur_status.emplace_back(1);
        } else if (std::abs(angle - prev_angle) >= threshold) {
            cur_status.emplace_back(angle > prev_angle);
        }

        prev_angle = angle;

        return remove_noise(cur_status);
    }

    bool remove_noise(std::vector<int> &cur_status) {
        int len = cur_status.size();

        // 010 || 101 => 000 | 111
        if (len > 2 && cur_status[len - 1] == cur_status[len - 3] && cur_status[len - 2] != cur_status[len - 1]) {
            cur_status[len - 2] = cur_status[len - 1];
        }

        // 00110 || 11001 => 00000 | 11111
        // for (int i = 3; i < len - 2; i++) {
        //     if (cur_status[i] == cur_status[i - 1] && cur_status[i + 1] == cur_status[i - 2]
        //         && cur_status[i] != cur_status[i + 1]) {
        //         cur_status[i] = cur_status[i - 1] = cur_status[i + 1];
        //     }
        // }

        // 00...1100 || 11...0011
        if (len > 5 && cur_status[len - 1] == cur_status[len - 2] && cur_status[len - 1] == cur_status[0]
            && cur_status[len - 1] != cur_status[len - 3]) {
            for (int i = 0; i < len - 2; i++) { spdlog::trace("{}", cur_status[i]); }
            return true;
        }

        return false;
    }

    void reset_status(const int track_id) {
        for (auto &[_, item] : status_[track_id]) {
            if (!item.empty()) { item.erase(item.begin(), item.end() - 1); }
        }
    }

private:
    std::map<int, std::map<int, std::vector<int>>> status_;
    std::map<int, std::map<int, nodes::PoseKeyPoint>> prev_key_points_;
    std::map<int, std::map<int, float>> prev_distances_;
    std::map<int, std::map<int, float>> prev_angles_;
};

NaiveCounter::NaiveCounter(std::vector<NaiveConfigItem> config)
    : config_(std::move(config)), up_impl_(std::make_unique<NaiveCounterPrivate>(config_.size())) {}

NaiveCounter::~NaiveCounter() {}

void NaiveCounter::update_key_point(const int track_id, int width, int height,
                                    const std::vector<nodes::PoseKeyPoint> &key_points,
                                    std::vector<std::vector<nodes::PoseKeyPoint>> &action_key_points) {
    int index = 0;
    for (const auto &item : config_) {
        auto &cur_track_key_points = track_key_points_[track_id][index];

        bool interval = false;
        int pixel_size = item.threshold * height;
        if (item.method == NavieMethod::kPoint_X) {
            interval = up_impl_->update_one_point_x(track_id, index, key_points[item.landmarks[0]], pixel_size);
        } else if (item.method == NavieMethod::kPoint_Y) {
            interval = up_impl_->update_one_point_y(track_id, index, key_points[item.landmarks[0]], pixel_size);
        } else if (item.method == NavieMethod::kDistance) {
            interval = up_impl_->update_distance(track_id, index, key_points[item.landmarks[0]],
                                                 key_points[item.landmarks[1]], pixel_size);
        } else if (item.method == NavieMethod::kAngle) {
            interval = up_impl_->update_angle(track_id, index, key_points[item.landmarks[0]],
                                              key_points[item.landmarks[1]], key_points[item.landmarks[3]], pixel_size);
        }

        cur_track_key_points.emplace_back(key_points);

        if (interval) {
            if (item.method == NavieMethod::kDistance) {
                auto &cmp_key_points = cur_track_key_points.back();
                // 判定两个点距离必须小于阈值
                auto distance =
                    square_of_distance(cmp_key_points[item.landmarks[0]], cmp_key_points[item.landmarks[1]]);
                if (distance > (height * height * 0.20)) {
                    spdlog::info("================================= continue");
                    continue;
                }
            }

            action_key_points = std::vector<std::vector<nodes::PoseKeyPoint>>(cur_track_key_points.begin(),
                                                                              cur_track_key_points.end() - 2);

            spdlog::info("================================= interval: {}", action_key_points.size());

            for (auto &[_, item] : track_key_points_[track_id]) {
                if (!item.empty()) { item.erase(item.begin(), item.end() - 1); };
            }
            up_impl_->reset_status(track_id);
            break;
        }

        ++index;
    }
}

}// namespace gddi