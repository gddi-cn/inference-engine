//
// Created by zhdotcai on 2022/12/5.
//

#ifndef __ROI_CROP_NODE_V2__
#define __ROI_CROP_NODE_V2__

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"

namespace gddi {
namespace nodes {
class RoiCrop_v2 : public node_any_basic<RoiCrop_v2> {
private:
    message_pipe<msgs::cv_frame> output_image_;

public:
    explicit RoiCrop_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("regions", regions_, "区域");
        bind_simple_property("regions_with_label", regions_with_label_, "区域");

        register_input_message_handler_(&RoiCrop_v2::on_cv_image_, this);
        output_image_ = register_output_message_<msgs::cv_frame>();
    }
    ~RoiCrop_v2() = default;

protected:
    void on_setup() override;
    void on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    // 按坐标比例
    std::vector<std::vector<std::vector<float>>> regions_;
    std::map<std::string, std::vector<std::vector<float>>> regions_with_label_;
    std::vector<Rect2f> vec_rects_;
    std::map<std::string, std::vector<Point2i>> roi_points_;
};
}// namespace nodes
}// namespace gddi

#endif// __ROI_CROP_NODE_V2__
