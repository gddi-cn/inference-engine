#include "mosaic_node_v2.h"
#include "utils.hpp"

namespace gddi {
namespace nodes {

void Mosaic_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == FrameType::kNone) {
        output_result_(frame);
        return;
    }

    auto &last_ext_info = frame->frame_info->ext_info.back();

    if (check_bbox_) {
        for (const auto &[idx, bbox] : last_ext_info.map_target_box) {
            last_ext_info.mosaic_rects.emplace_back(bbox.box);
        }
    }

    if (check_key_point_) {
        for (const auto &[idx, key_points] : last_ext_info.map_key_points) {
            float width = last_ext_info.map_target_box.at(idx).box.width;
            last_ext_info.mosaic_rects.emplace_back(Rect2f{.x = (key_points[3].x + key_points[4].x) / 2 - width / 6,
                                                           .y = (key_points[3].y + key_points[4].y) / 2 - width / 5,
                                                           .width = width / 3,
                                                           .height = width / (float)2.5});
        }
    }

    output_result_(frame);
}

}// namespace nodes
}// namespace gddi
