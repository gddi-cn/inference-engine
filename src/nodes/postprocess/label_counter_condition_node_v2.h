/**
 * @file target_analyzer_node_v2.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 0.1
 * @date 2023-05-24
 * 
 * @copyright Copyright (c) 2023 by GDDI
 * 
 */

#pragma once

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"
#include <chrono>
#include <cstdint>
#include <map>
#include <vector>

namespace gddi {
namespace nodes {

class LabelCounterCondition_v2 : public node_any_basic<LabelCounterCondition_v2> {
private:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit LabelCounterCondition_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("label", label_, "标签");
        bind_simple_property("operator", operator_, "运算符");
        bind_simple_property("threshold", threshold_, "阈值");

        register_input_message_handler_(&LabelCounterCondition_v2::on_cv_image, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }
    ~LabelCounterCondition_v2() = default;

protected:
    void on_setup() override;
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    std::string label_;
    std::string operator_;
    uint32_t threshold_{1};
};

}// namespace nodes
}// namespace gddi