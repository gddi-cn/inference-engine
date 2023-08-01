#ifndef __FFT_COUNTER_H__
#define __FFT_COUNTER_H__

#include "node_struct_def.h"
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace gddi {

enum class Orientation { kVertical, kHorizontal, kScore };

struct PosePoint {
    float weight;
    Orientation orientation;
};

class FFTCounter {
public:
    FFTCounter(std::map<int, PosePoint> points, int sliding_window);
    int update(const int track_id, const int64_t frame_idx, const std::vector<nodes::PoseKeyPoint> &key_points);
    void get_range(const int track_id, std::map<int64_t, std::vector<nodes::PoseKeyPoint>> &action_key_point);

protected:
    int fft_count(const std::vector<double> &data);

private:
    int frame_rate_;
    int sliding_window_;
    std::map<int, PosePoint> pose_points_;
    std::map<int, std::vector<std::pair<int64_t, std::vector<nodes::PoseKeyPoint>>>> cache_key_points_;

    std::map<int, int> last_range_len_;// 累计统计区间
    std::map<int, int> erase_count_;   // 累计删除个数

    std::map<int, std::vector<int>> range_;
};

class ActionPair {
public:
    ActionPair(std::map<int, PosePoint> points);
    double pair_action(const std::map<int, std::vector<nodes::PoseKeyPoint>> &current_range,
                       const std::vector<std::vector<nodes::PoseKeyPoint>> &standard_range,
                       std::map<std::pair<int, int>, std::vector<double>> &pairs);

private:
    std::map<int, PosePoint> pose_points_;
};

void normalize_keypoints(const std::vector<nodes::PoseKeyPoint> &key_points);

}// namespace gddi
#endif