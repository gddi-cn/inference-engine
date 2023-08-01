#include "target_status_node_v2.h"
#include "modules/cvrelate/geometry.h"
#include "spdlog/spdlog.h"

namespace gddi {
namespace nodes {

void TargetStatus_v2::on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame) {
    for (const auto &[track_id, track_info] : frame->frame_info->ext_info.back().tracked_box) {
        if (prev_tracked_points_.count(track_id) == 0) {
            prev_tracked_points_[track_id] = std::vector<Point2i>{
                {int(track_info.box.x), int(track_info.box.y)},
                {int(track_info.box.x), int(track_info.box.y + track_info.box.height)},
                {int(track_info.box.x + track_info.box.width), int(track_info.box.y + track_info.box.height)},
                {int(track_info.box.x + track_info.box.width), int(track_info.box.y)},
            };
            continue;
        }

        auto prev_points = prev_tracked_points_.at(track_id);
        std::vector<Point2i> curr_points{
            {int(track_info.box.x), int(track_info.box.y)},
            {int(track_info.box.x), int(track_info.box.y + track_info.box.height)},
            {int(track_info.box.x + track_info.box.width), int(track_info.box.y + track_info.box.height)},
            {int(track_info.box.x + track_info.box.width), int(track_info.box.y)},
        };

        if (geometry::area_cover_rate(prev_points, curr_points) >= iou_threshold_) {
            if (stating_) {
                event_group_[track_id].emplace_back(1);
                break;
            } else if (moving_) {
                event_group_[track_id].emplace_back(0);
                break;
            }
        } else {
            if (stating_) {
                event_group_[track_id].emplace_back(0);
                break;
            } else if (moving_) {
                event_group_[track_id].emplace_back(1);
                break;
            }
        }
    }

    frame->check_report_callback_ = [](const std::vector<FrameExtInfo> &) { return FrameType::kBase; };
    for (auto iter = event_group_.begin(); iter != event_group_.end();) {
        if (iter->second.size() >= int(interval_ * frame->infer_frame_rate)) {
            float count = 0;
            for (const auto &event : iter->second) {
                if (event) { ++count; }
            }

            if (count / iter->second.size() >= cnt_threshold_) {
                if (iter->second.back() != 1) { break; }
                frame->check_report_callback_ = [](const std::vector<FrameExtInfo> &) { return FrameType::kReport; };
            }

            prev_tracked_points_.erase(iter->first);
            iter = event_group_.erase(iter);
        } else {
            ++iter;
        }
    }

    output_image_(frame);
}

}// namespace nodes
}// namespace gddi