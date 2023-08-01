/**
 * @file regional_counter_node_v2.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 0.1
 * @date 2023-06-13
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
#include <ctime>

namespace gddi {
namespace nodes {
class RegionalCounter_v2 : public node_any_basic<RegionalCounter_v2> {
private:
    message_pipe<msgs::cv_frame> output_image_;

public:
    explicit RegionalCounter_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("region_width", region_width_ratio_, "区域宽度");
        bind_simple_property("region_height", region_height_ratio_, "区域高度");
        bind_simple_property("threshold", threshold_, "区域目标数量阈值");

        register_input_message_handler_(&RegionalCounter_v2::on_cv_image_, this);
        output_image_ = register_output_message_<msgs::cv_frame>();
    }
    ~RegionalCounter_v2() = default;

protected:
    void on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    float region_width_ratio_{0.3}; // 区域宽度
    float region_height_ratio_{0.3};// 区域高度
    uint32_t threshold_{3};         // 区域内目标数量

    std::vector<std::vector<int>> center_point_image;
};

}// namespace nodes
}// namespace gddi
