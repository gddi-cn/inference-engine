/**
 * @file target_tracker.h
 * @author zhdotcai
 * @brief 
 * @version 0.1
 * @date 2022-12-06
 * 
 * @copyright Copyright (c) 2022 GDDI
 * 
 */

#ifndef __GDDI_TARGET_TRACKER_H__
#define __GDDI_TARGET_TRACKER_H__

#include "types.hpp"
#include <map>
#include <memory>
#include <vector>

namespace gddi {

struct TrackObject {
    int target_id;
    int class_id;
    float prob;
    Rect2f rect;
};

struct TrackOption {
    float track_thresh{0.5};// 跟踪阈值
    float high_thresh{0.6}; // 大于该阈值的框进入高队列
    float match_thresh{0.8};// 大于该阈值的框直接匹配
    int max_frame_lost{30}; // 最大丢失帧数
};

class TargetTrackerPrivate;

class TargetTracker {
public:
    /**
     * @brief Construct a new Target Tracker object
     * 
     * @param option 
     */
    TargetTracker(const TrackOption &option);
    ~TargetTracker();

    /**
     * @brief Update position
     * 
     * @param objects 
     * @return std::map<int, TrackObject> 
     */
    std::map<int, TrackObject> update_objects(const std::vector<TrackObject> &objects);

private:
    std::unique_ptr<TargetTrackerPrivate> uq_impl_;
};

};// namespace gddi

#endif