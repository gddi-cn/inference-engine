//
// Created by cc on 2021/11/5.
//

#ifndef __TARGET_TRACKER_NODE_V2_HPP__
#define __TARGET_TRACKER_NODE_V2_HPP__

#include "bytetrack/target_tracker.h"
#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include <mutex>

namespace gddi {
namespace nodes {

struct TrackStatus {
    int pre_class_id;
    uint32_t prev_timestamp;
    uint32_t last_timestamp;
    std::vector<int> class_ids;
};

class TargetTracker_v2 : public node_any_basic<TargetTracker_v2> {
protected:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit TargetTracker_v2(std::string name) : node_any_basic<TargetTracker_v2>(std::move(name)) {
        register_input_message_handler_(&TargetTracker_v2::on_cv_frame, this);

        bind_simple_property("continuous_tracking_time", continuous_tracking_time_, "持续跟踪时间(s)");
        bind_simple_property("max_lost_time", max_lost_time_, "目标丢失时间(s)");
        bind_simple_property("appear_report", appear_report_, "目标出现上报");
        bind_simple_property("disappear_report", disappear_report_, "目标消失上报");
        bind_simple_property("lost_report", disappear_report_, "目标消失上报");// 兼容旧字段

        output_result_ = register_output_message_<msgs::cv_frame>();
    }

    ~TargetTracker_v2() override = default;

private:
    void on_setup() override;

    void on_cv_frame(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    uint32_t continuous_tracking_time_{0};
    uint32_t max_lost_time_{4};
    bool appear_report_{true};
    bool disappear_report_{false};

    std::shared_ptr<TargetTracker> tracking_;
    std::map<int, TrackStatus> target_status_;// track_id - class_ids
};

}// namespace nodes
}// namespace gddi

#endif
