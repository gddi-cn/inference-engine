#include "check_moving_node_v2.h"
#include <algorithm>

namespace gddi {
namespace nodes {

float calculate_iou(const Rect2f &bbox1, const Rect2f &bbox2) {
    float x1 = std::max(bbox1.x, bbox2.x);
    float y1 = std::max(bbox1.y, bbox2.y);
    float x2 = std::min(bbox1.x + bbox1.width, bbox2.x + bbox2.width);
    float y2 = std::min(bbox1.y + bbox1.height, bbox2.y + bbox2.height);

    float width = std::max(0.0f, x2 - x1 + 1);
    float height = std::max(0.0f, y2 - y1 + 1);
    float inter_area = width * height;

    float bbox1_area = bbox1.width * bbox1.height;
    float bbox2_area = bbox2.width * bbox2.height;

    float union_area = bbox1_area + bbox2_area - inter_area;

    return inter_area / union_area;
}

void CheckMoving_v2::on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == FrameType::kNone) {
        output_image_(frame);
        return;
    }

    for (auto &[target_id, info] : frame->frame_info->ext_info.back().map_target_box) {
        auto iter = prev_bbox_.begin();
        for (; iter != prev_bbox_.end();) {
            if (calculate_iou(iter->second.box, info.box) > threshold_) {
                break;
            } else {
                ++iter;
            }
        }

        if (iter == prev_bbox_.end()) {
            info.moving = true;
            frame->check_report_callback_ = [](const std::vector<FrameExtInfo> &) { return FrameType::kReport; };
            spdlog::debug("target {} is moving", target_id);
        } else {
            frame->check_report_callback_ = [](const std::vector<FrameExtInfo> &) { return FrameType::kBase; };
        }
    }

    prev_bbox_ = frame->frame_info->ext_info.back().map_target_box;

    output_image_(frame);
}

}// namespace nodes
}// namespace gddi