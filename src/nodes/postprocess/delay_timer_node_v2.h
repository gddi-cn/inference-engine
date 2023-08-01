/**
 * @file delay_timer_node_v2.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 0.1
 * @date 2023-06-16
 * 
 * @copyright Copyright (c) 2023 by GDDI
 * 
 */

#pragma once

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"

namespace gddi {
namespace nodes {
class DelayTimer_v2 : public node_any_basic<DelayTimer_v2> {
private:
    message_pipe<msgs::cv_frame> output_image_;

public:
    explicit DelayTimer_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("duration", duration_, "时长");

        register_input_message_handler_(&DelayTimer_v2::on_cv_image_, this);
        output_image_ = register_output_message_<msgs::cv_frame>();
    }
    ~DelayTimer_v2() = default;

protected:
    void on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    uint32_t duration_{3};
    time_t event_time_{std::numeric_limits<time_t>::max()};
};

}// namespace nodes
}// namespace gddi