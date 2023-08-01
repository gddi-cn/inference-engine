//
// Created by zhdotcai on 2022/1/8.
//

#ifndef INFERENCE_ENGINE_ROI_NODE_V1_HPP
#define INFERENCE_ENGINE_ROI_NODE_V1_HPP

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"

namespace gddi {
namespace nodes {
class RoiFilter_v2 : public node_any_basic<RoiFilter_v2> {
private:
    message_pipe<msgs::cv_frame> output_image_;

public:
    explicit RoiFilter_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("regions", regions_, "区域");
        bind_simple_property("regions_with_label", regions_with_label_, "区域");
        bind_simple_property("threshold", threshold_, "阈值");

        bind_simple_flags("support_preview", true);

        register_input_message_handler_(&RoiFilter_v2::on_cv_image_, this);
        output_image_ = register_output_message_<msgs::cv_frame>();
    }
    ~RoiFilter_v2() = default;

protected:
    void on_setup() override;
    void on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    // 按坐标比例
    std::vector<std::vector<std::vector<float>>> regions_;
    std::map<std::string, std::vector<std::vector<float>>> regions_with_label_;
    std::map<std::string, std::vector<Point2i>> roi_points_;
    float threshold_{0.5};
};
}// namespace nodes
}// namespace gddi

#endif// INFERENCE_ENGINE_ROI_NODE_V1_HPP
