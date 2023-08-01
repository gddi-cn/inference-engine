//
// Created by zhdotcai on 2022/01/19.
//

#ifndef __SEG_CALCULATION_NODE_V2_H__
#define __SEG_CALCULATION_NODE_V2_H__

#include "message_templates.hpp"
#include "modules/postprocess/seg_area.h"
#include "modules/postprocess/seg_volume.h"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"
#include <memory>
#include <vector>

namespace gddi {
namespace nodes {
class SegCalculation_v2 : public node_any_basic<SegCalculation_v2> {
private:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit SegCalculation_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("param_matrix", param_matrix_, "参数矩阵");
        bind_simple_property("enable_area", enable_area_, "计算面积");
        bind_simple_property("enable_length", enable_length_, "计算长度");
        bind_simple_property("enable_volume", enable_volume_, "计算体积");
        bind_simple_property("area_thresh", area_thresh_, "面积阈值");

        register_input_message_handler_(&SegCalculation_v2::on_cv_image, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }

protected:
    void on_setup() override;
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    std::string param_matrix_;
    bool enable_area_{false};
    bool enable_length_{false};
    bool enable_volume_{false};
    double area_thresh_{10000};
    
    std::unique_ptr<BevTransform> bev_trans_;
    std::unique_ptr<SegAreaAndMaxLengthByContours> seg_by_contours_;
    std::unique_ptr<CameraDistort> camera_distort_;
    std::map<std::string, std::unique_ptr<SegVolume>> seg_volumes_;
};
}// namespace nodes
}// namespace gddi

#endif// __SEG_COMPUTATION_NODE_V2_H__
