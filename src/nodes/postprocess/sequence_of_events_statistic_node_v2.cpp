#include "sequence_of_events_statistic_node_v2.h"
#include "node_struct_def.h"
#include "spdlog/spdlog.h"

namespace gddi {
namespace nodes {

void SequenceOfEventsStatistic_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    event_group_.push_back(frame->frame_info->frame_event_result);

    if (frame->frame_info->frame_event_result) {
        last_one_frame_ = frame;
    } else {
        last_zero_frame_ = frame;
    }

    EventChange result{EventChange::kZeroToZero};
    if (event_group_.size() >= int(interval_ * frame->infer_frame_rate)) {
        float count = 0;
        for (const auto &event : event_group_) {
            if (event) { ++count; }
        }

        // threshold 可以为 0
        if (count / event_group_.size() >= threshold_) {
            result = process_group_status(1);
        } else {
            result = process_group_status(0);
        }

        event_group_.clear();
    }

    if (zero_to_one_ && result == EventChange::kZeroToOne) {
        spdlog::debug("event happened report");
        last_one_frame_->check_report_callback_ = [](const std::vector<FrameExtInfo> &) { return FrameType::kReport; };
        output_result_(last_one_frame_);
    } else if (one_to_one_ && result == EventChange::kOneToOne) {
        spdlog::debug("event continue report");
        last_one_frame_->check_report_callback_ = [](const std::vector<FrameExtInfo> &) { return FrameType::kReport; };
        output_result_(last_one_frame_);
    } else if (one_to_zero_ && result == EventChange::kOneToZero) {
        spdlog::debug("event end report");
        last_zero_frame_->check_report_callback_ = [](const std::vector<FrameExtInfo> &) { return FrameType::kReport; };
        output_result_(last_zero_frame_);
    } else {
        frame->check_report_callback_ = [](const std::vector<FrameExtInfo> &) { return FrameType::kBase; };
        output_result_(frame);
    }
}

EventChange SequenceOfEventsStatistic_v2::process_group_status(int new_group_status) {

    auto group_status = last_group_status_;

    last_group_status_ = new_group_status;

    if (group_status == 0 && new_group_status == 1) {
        spdlog::debug("event happened");

        one_one_times_ = times_;
        one_one_time_ = 0;

        return EventChange::kZeroToOne;
    } else if (group_status == 1 && new_group_status == 1) {
        spdlog::debug("event continue");

        one_one_time_ += interval_;
        if (one_one_time_ >= hold_time_ && one_one_times_ > 0) {
            --one_one_times_;
            one_one_time_ = 0;
            return EventChange::kOneToOne;
        }
    } else if (group_status == 1 && new_group_status == 0) {
        spdlog::debug("event end");

        return EventChange::kOneToZero;
    } else {
        spdlog::debug("event not happen");
    }

    return EventChange::kZeroToZero;
}

}// namespace nodes
}// namespace gddi
