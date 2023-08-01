//
// Created by zhdotcai on 2022/1/8.
//

#ifndef __LOGIC_GATE_V2__
#define __LOGIC_GATE_V2__

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"

namespace gddi {
namespace nodes {
class LogicGate_v2 : public node_any_basic<LogicGate_v2> {
private:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit LogicGate_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("operation", operation_, {"AND", "OR", "NOT", "XOR"}, "运算逻辑");
        bind_simple_property("labels", labels_, "目标标签");
        bind_simple_property("duration", duration_, "持续时间");

        register_input_message_handler_(&LogicGate_v2::on_cv_image, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }
    ~LogicGate_v2() = default;

protected:
    void on_setup() override;
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    std::string operation_;
    std::set<std::string> labels_;
    uint32_t duration_{1};

    std::chrono::steady_clock::time_point last_time_point_;
};
}// namespace nodes
}// namespace gddi

#endif// __LOGIC_GATE_V2__
