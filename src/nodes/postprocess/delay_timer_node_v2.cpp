#include "delay_timer_node_v2.h"

namespace gddi {
namespace nodes {

void DelayTimer_v2::on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == FrameType::kNone) {
        output_image_(frame);
        return;
    }

    if (frame->check_report_callback_(frame->frame_info->ext_info) == FrameType::kReport) {
        if (event_time_ == std::numeric_limits<time_t>::max()) {
            event_time_ = time(NULL);
        }
    }

    if (time(NULL) - event_time_ >= duration_) {
        event_time_ = std::numeric_limits<time_t>::max();
        frame->check_report_callback_ = [this](const std::vector<FrameExtInfo> &) { return FrameType::kReport; };
    } else {
        frame->check_report_callback_ = [this](const std::vector<FrameExtInfo> &) { return FrameType::kBase; };
    }

    output_image_(frame);
}

}// namespace nodes
}// namespace gddi