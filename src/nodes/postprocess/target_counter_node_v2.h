//
// Created by zhdotcai on 2022/01/19.
//

#ifndef __TARGET_COUNTER_NODE_V2_H__
#define __TARGET_COUNTER_NODE_V2_H__

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "postprocess/camera_status_node_v2.h"
#include "types.hpp"
#include <bits/types/time_t.h>
#include <map>
#include <memory>
#include <vector>

namespace gddi {
namespace nodes {

// 摄像头状态
enum class CameraStatus { kStaing, kMoving };

class TargetCounter_v2 : public node_any_basic<TargetCounter_v2> {
private:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit TargetCounter_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("counting_logic", counting_logic_, "计数逻辑");
        bind_simple_property("counting_interval", counting_interval_, "统计间隔(s)");
        bind_simple_property("staing_time", staing_time_, "停留时间(s)");

        bind_simple_property("counting_times", counting_times_, "统计次数(天)");

        register_input_message_handler_(&TargetCounter_v2::on_cv_image, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }

protected:
    void on_setup() override;
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    std::string counting_logic_;
    uint32_t counting_interval_{15};
    uint32_t staing_time_{150};
    uint32_t counting_times_{UINT32_MAX};

    uint32_t moving_threshold_{25};
    uint32_t staing_interval_{14};

    // 标签投票计数
    std::map<std::string, uint32_t> labele_counts_;
    CameraStatus prev_status_{CameraStatus::kStaing};
    time_t prev_moving_time_{time(NULL) + 24 * 60 * 60};

    nlohmann::json counting_logic_obj_;

    uint32_t counter_index_{1};
    std::string index_label_;
    std::map<std::string, uint32_t> target_counts_;

#if defined(WITH_NVIDIA)
    cv::cuda::GpuMat prev_frame_;
#else
    cv::Mat prev_frame_;
#endif
};

}// namespace nodes
}// namespace gddi

#endif// __TARGET_COUNTER_NODE_V2_H__
