/**
 * @file crop_image_node_v3.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 0.1
 * @date 2022-12-15
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __CROP_IMAGE_NODE_V3_HPP__
#define __CROP_IMAGE_NODE_V3_HPP__

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include <cstdint>

namespace gddi {
namespace nodes {
class CropImage_v3 : public node_any_basic<CropImage_v3> {
protected:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit CropImage_v3(std::string name) : node_any_basic<CropImage_v3>(std::move(name)) {
        register_input_message_handler_(&CropImage_v3::on_cv_image, this);

        bind_simple_property("expansion_factor", expansion_factor_, 0, 10, "膨胀系数");
        bind_simple_property("interlaced_number", interlaced_number_, 0, UINT16_MAX, "隔帧数");
        bind_simple_property("max_target_number", max_target_number_, 1, UINT16_MAX, "最多扣图目标数");

        output_result_ = register_output_message_<msgs::cv_frame>();
    }

    ~CropImage_v3() override = default;

private:
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    float expansion_factor_ = 1.0;
    size_t interlaced_number_ = 4;
    size_t max_target_number_ = 8;

    uint64_t frame_idx_{0};
};
}// namespace nodes
}// namespace gddi

#endif//__CROP_IMAGE_NODE_V2_HPP__
