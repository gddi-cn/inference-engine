#ifndef TRAJECTORY_NODE_HPP_
#define TRAJECTORY_NODE_HPP_

#include <cassert>
#include <map>

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"

namespace gddi {
namespace nodes {

class Trajectory : public node_any_basic<Trajectory> {
public:
    explicit Trajectory(std::string name) : node_any_basic<Trajectory>(std::move(name)) {
        bind_simple_property("frame_interval", frame_interval_, "frame_interval");
        bind_simple_property("motion_dis_thres", motion_dis_thres_, "motion_dis_thres");
        bind_simple_property("radian_thres", radian_thres_, "radian_thres");

        register_input_message_handler_(&Trajectory::on_image_, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }

protected:
    message_pipe<msgs::cv_frame> output_result_;
    // void on_setup() override {

    // }

    void on_image_(const std::shared_ptr<msgs::cv_frame> &frame) {

        std::cout << "------------- " << frame->frame_info->frame_idx << " "
                  << frame->frame_info->basic_specific_msg->name() << std::endl;
        frame->frame_info->basic_specific_msg->set_data(frame->frame_info);
        trajectory_result_ = frame->frame_info->basic_specific_msg->trajectory(
            frame_interval_, motion_dis_thres_, radian_thres_);

        output_result_(frame);
    }

public:
    std::map<int, std::vector<std::pair<int, std::string>>> trajectory_result_;

private:
    int frame_interval_;
    float motion_dis_thres_;
    float radian_thres_;
};

}// namespace nodes
}// namespace gddi

#endif// end TRAJECTORY_NODE_HPP_
