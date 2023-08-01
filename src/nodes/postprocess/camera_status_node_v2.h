/**
 * @file camera_status_node_v2.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 0.1
 * @date 2023-03-16
 * 
 * @copyright Copyright (c) 2023 by GDDI
 * 
 */

#ifndef __CAMERA_STATUS_NODE_V2_H__
#define __CAMERA_STATUS_NODE_V2_H__

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"

namespace gddi {
namespace nodes {

class CameraStatus_v2 : public node_any_basic<CameraStatus_v2> {
protected:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit CameraStatus_v2(std::string name) : node_any_basic<CameraStatus_v2>(std::move(name)) {
        bind_simple_property("moving_threshold", moving_threshold_, "移动阈值");
        bind_simple_property("staing_interval", staing_interval_, "停留间隔(s)");
        bind_simple_property("staing_time", staing_time_, "停留时间(s)");

        register_input_message_handler_(&CameraStatus_v2::on_cv_image, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }

    ~CameraStatus_v2() override = default;

private:
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    uint32_t moving_threshold_{20};
    uint32_t staing_time_{0};
    uint32_t staing_interval_{0};

    // CameraStatus prev_status_{CameraStatus::kStaing};
    time_t prev_moving_time_{time(NULL)};

#if defined(WITH_NVIDIA)
    cv::cuda::GpuMat prev_frame_;
#else
    cv::Mat prev_frame_;
#endif
};

}// namespace nodes
}// namespace gddi

#endif//__BOX_FILTER_NODE_V2_HPP__
