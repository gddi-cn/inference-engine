/**
 * @file direction_node_v2.hpp
 * @author zhdotcai
 * @brief 
 * @version 0.1
 * @date 2022-08-11
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __DIRECTION_NODE_V2_HPP__
#define __DIRECTION_NODE_V2_HPP__

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"

namespace gddi {
namespace nodes {

struct TrackPoint {
    Point2f point;
    float offset_distance;
    float offset_angle;
};

class Direction_v2 : public node_any_basic<Direction_v2> {
protected:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit Direction_v2(std::string name) : node_any_basic<Direction_v2>(std::move(name)) {
        register_input_message_handler_(&Direction_v2::on_cv_frame, this);

        bind_simple_property("direction", direction_, {0, 45, 90, 135, 180, 225, 270, 360}, "方向");
        bind_simple_property("distance_thresh", distance_thresh_, "间距阈值");
        bind_simple_property("duration", duration_, "计算周期");

        output_result_ = register_output_message_<msgs::cv_frame>();
    }

private:
    void on_setup() override;
    void on_cv_frame(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    float direction_;
    float distance_thresh_{0.05};
    int duration_{2};

    time_t prev_timestamp_;
    std::map<int, std::vector<TrackPoint>> track_direction_;
};

}// namespace nodes
}// namespace gddi

#endif
