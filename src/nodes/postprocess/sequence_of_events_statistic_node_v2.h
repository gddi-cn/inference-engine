/**
 * @file sequence_of_events_statistic_node_v2.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 0.1
 * @date 2023-05-25
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

enum class EventChange {
    kZeroToOne = 0,
    kOneToOne = 1,
    kOneToZero = 2,
    kZeroToZero = 3,
};

class SequenceOfEventsStatistic_v2 : public node_any_basic<SequenceOfEventsStatistic_v2> {
private:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit SequenceOfEventsStatistic_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("interval", interval_, "间隔");
        bind_simple_property("zero_to_one", zero_to_one_, "事件发生");
        bind_simple_property("one_to_one", one_to_one_, "事件持续发生");
        bind_simple_property("one_to_zero", one_to_zero_, "事件结束");
        bind_simple_property("threshold", threshold_, "阈值");
        bind_simple_property("hold_time", hold_time_, "持续时间");
        bind_simple_property("times", times_, "次数");

        register_input_message_handler_(&SequenceOfEventsStatistic_v2::on_cv_image, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }
    ~SequenceOfEventsStatistic_v2() = default;

protected:
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);

    EventChange process_group_status(int new_group_status);

private:
    float interval_{0};
    bool zero_to_one_{false};
    bool one_to_one_{false};
    bool one_to_zero_{false};

    float hold_time_{0};
    float threshold_{0.5};
    uint32_t times_{1};

    std::vector<int> event_group_;

    int last_group_status_{0};

    float one_one_time_{0};
    uint32_t one_one_times_{0};

    std::shared_ptr<msgs::cv_frame> last_one_frame_;
    std::shared_ptr<msgs::cv_frame> last_zero_frame_;
};

}// namespace nodes
}// namespace gddi