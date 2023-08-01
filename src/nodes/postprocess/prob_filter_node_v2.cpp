#include "prob_filter_node_v2.h"
#include "node_msg_def.h"
#include <spdlog/spdlog.h>

namespace gddi {
namespace nodes {

void ProbFilter_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == FrameType::kNone) {
        output_result_(frame);
        return;
    }

    auto clone_frame = std::make_shared<msgs::cv_frame>(frame);
    auto &ext_info = clone_frame->frame_info->ext_info.back();

    for (auto iter = ext_info.map_target_box.begin(); iter != ext_info.map_target_box.end();) {
        if (iter->second.prob < box_prob_) {
            ext_info.map_key_points.erase(iter->first);
            iter = ext_info.map_target_box.erase(iter);
        } else {
            ++iter;
        }
    }

    // for (auto iter = ext_info.map_key_points.begin(); iter != ext_info.map_key_points.end();) {
    //     for (auto point_iter = iter->second.begin(); point_iter != iter->second.end();) {
    //         if (point_iter->prob < key_point_prob_) {
    //             point_iter = iter->second.erase(point_iter);
    //         } else {
    //             ++point_iter;
    //         }
    //     }
    //     if (iter->second.size() < 17) {
    //         iter = ext_info.map_key_points.erase(iter);
    //     } else {
    //         ++iter;
    //     }
    // }

    output_result_(clone_frame);
}

}// namespace nodes
}// namespace gddi
