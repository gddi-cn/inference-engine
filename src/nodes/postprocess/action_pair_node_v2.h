//
// Created by zhdotcai on 2022/01/19.
//

#ifndef __ACTION_PAIR_NODE_V2_H__
#define __ACTION_PAIR_NODE_V2_H__

#include "message_templates.hpp"
#include "modules/postprocess/fft_counter.h"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"
#include <map>
#include <memory>

namespace gddi {
namespace nodes {
class ActionPair_v2 : public node_any_basic<ActionPair_v2> {
private:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit ActionPair_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("action_param", action_param_, "动作参数");
        bind_simple_property("standard_file", standard_file_, "标准动作配置文件");
        bind_simple_property("threshold", threshold_, "阈值");

        bind_simple_flags("support_preview", true);

        register_input_message_handler_(&ActionPair_v2::on_cv_image, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }

protected:
    void on_setup() override;
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    std::string action_param_;
    std::string standard_file_;
    float threshold_{0};

    std::unique_ptr<gddi::ActionPair> action_pair_;
    std::vector<std::vector<gddi::nodes::PoseKeyPoint>> standard_range_;

    std::map<int, std::map<int, std::vector<float>>> action_score_;
};
}// namespace nodes
}// namespace gddi

#endif// __FFT_ACTION_PAIR_NODE_V2_H__
