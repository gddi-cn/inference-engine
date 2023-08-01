/**
 * @file naive_counter.h
 * @author zhdotcai
 * @brief 
 * @version 0.1
 * @date 2022-12-12
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __NAIVE_COUNTER__
#define __NAIVE_COUNTER__

#include "node_struct_def.h"
#include <map>
#include <memory>
#include <vector>

namespace gddi {

enum class NavieMethod { kPoint_X = 0, kPoint_Y, kDistance, kAngle };

struct NaiveConfigItem {
    NavieMethod method;
    std::vector<int> landmarks;
    float threshold{0.01};
};

class NaiveCounterPrivate;

class NaiveCounter {
public:
    NaiveCounter(std::vector<NaiveConfigItem> config);
    ~NaiveCounter();

    void update_key_point(const int track_id, int width, int height, const std::vector<nodes::PoseKeyPoint> &key_points,
                          std::vector<std::vector<nodes::PoseKeyPoint>> &action_key_points);

private:
    std::vector<NaiveConfigItem> config_;
    std::unique_ptr<NaiveCounterPrivate> up_impl_;

    std::map<int, std::map<int, std::vector<std::vector<nodes::PoseKeyPoint>>>> track_key_points_;
};

}// namespace gddi

#endif