/**
 * @file roi_perspective_transform_v2.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 0.1
 * @date 2023-05-23
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

class RoiPerspectiveTransform_v2 : public node_any_basic<RoiPerspectiveTransform_v2> {
private:
    message_pipe<msgs::cv_frame> output_image_;

public:
    explicit RoiPerspectiveTransform_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("regions_with_label", regions_with_label_, "区域");
        bind_simple_property("threshold", threshold_, "阈值");

        bind_simple_flags("support_preview", true);

        register_input_message_handler_(&RoiPerspectiveTransform_v2::on_cv_image_, this);
        output_image_ = register_output_message_<msgs::cv_frame>();
    }
    ~RoiPerspectiveTransform_v2() = default;

protected:
    void on_setup() override;
    void on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    float threshold_{0.5};
    std::map<std::string, std::vector<std::vector<float>>> regions_with_label_;
    std::map<std::string, std::vector<Point2i>> roi_points_;

    cv::Mat transform_matrix_;// 透视变换矩阵
    cv::Mat inverse_matrix_;  // 逆矩阵
};

}// namespace nodes
}// namespace gddi
