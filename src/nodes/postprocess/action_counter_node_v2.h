//
// Created by zhdotcai on 2022/01/19.
//

#ifndef __ACTION_COUNTER_NODE_V2_H__
#define __ACTION_COUNTER_NODE_V2_H__

#include "message_templates.hpp"
// #include "modules/postprocess/fft_counter.h"
#include "modules/postprocess/naive_counter.h"
#include "modules/postprocess/track_smoothing.h"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "types.hpp"
#include <memory>

namespace gddi {
namespace nodes {
class ActionCounter_v2 : public node_any_basic<ActionCounter_v2> {
private:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit ActionCounter_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("action_param", action_param_, "动作参数");
        // bind_simple_property("sliding_window", sliding_window_, "滑动窗口");

        register_input_message_handler_(&ActionCounter_v2::on_cv_image, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }

protected:
    void on_setup() override;
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    std::string action_param_;
    int sliding_window_{0};
    // std::map<int, gddi::PosePoint> standard_points_;

    // std::unique_ptr<gddi::FFTCounter> fft_couter_;
    std::unique_ptr<NaiveCounter> naive_couter_;
    std::unique_ptr<TrackSmoothing> track_smoother_;
};
}// namespace nodes
}// namespace gddi

#endif// __FFT_COUNTER_NODE_V2_H__
