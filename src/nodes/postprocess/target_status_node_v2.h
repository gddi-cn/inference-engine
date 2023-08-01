/**
 * @file target_staing_node_v2.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 0.1
 * @date 2023-06-27
 * 
 * @copyright Copyright (c) 2023 by GDDI
 * 
 */

#pragma once

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"
#include <cstdint>

namespace gddi {
namespace nodes {

class TargetStatus_v2 : public node_any_basic<TargetStatus_v2> {
private:
    message_pipe<msgs::cv_frame> output_image_;

public:
    explicit TargetStatus_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("iou_threshold", iou_threshold_, "IOU阈值");
        bind_simple_property("interval", interval_, "统计间隔");
        bind_simple_property("cnt_threshold", cnt_threshold_, "统计阈值");
        bind_simple_property("stating", stating_, "目标静止上报");
        bind_simple_property("moving", moving_, "目标移动上报");

        register_input_message_handler_(&TargetStatus_v2::on_cv_image_, this);
        output_image_ = register_output_message_<msgs::cv_frame>();
    }
    ~TargetStatus_v2() = default;

protected:
    void on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    float iou_threshold_{0.5};
    uint32_t interval_{10};
    float cnt_threshold_{0.5};
    bool stating_{true};
    bool moving_{false};

    std::map<int, std::vector<int>> event_group_;
    std::map<int, std::vector<Point2i>> prev_tracked_points_;
};

}// namespace nodes
}// namespace gddi
