#include "regional_counter_node_v2.h"
#include "modules/cvrelate/geometry.h"
#include "spdlog/spdlog.h"

namespace gddi {
namespace nodes {

const int kScaleFactor = 8;

void RegionalCounter_v2::on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == FrameType::kNone) {
        output_image_(frame);
        return;
    }

    frame->frame_info->frame_event_result = 0;

    int width = frame->frame_info->width() / kScaleFactor;
    int height = frame->frame_info->height() / kScaleFactor;
    int region_width = int(width * region_width_ratio_);
    int region_height = int(height * region_height_ratio_);

    if (center_point_image.empty()) {
        center_point_image = std::vector<std::vector<int>>(height, std::vector<int>(width, 0));
    }

    for (auto &row : center_point_image) { memset(row.data(), 0, row.size() * sizeof(int)); }

    for (const auto &[_, bbox] : frame->frame_info->ext_info.back().map_target_box) {
        center_point_image[int((bbox.box.y + bbox.box.height / 2) / kScaleFactor)]
                          [int((bbox.box.x + bbox.box.width / 2) / kScaleFactor)] = 1;
    }

    // 计算累积和矩阵
    std::vector<std::vector<int>> cumulative_sum(height + 1, std::vector<int>(width + 1, 0));
    for (int i = 1; i <= height; ++i) {
        for (int j = 1; j <= width; ++j) {
            cumulative_sum[i][j] = cumulative_sum[i - 1][j] + cumulative_sum[i][j - 1] - cumulative_sum[i - 1][j - 1]
                + center_point_image[i - 1][j - 1];
        }
    }

    for (int i = region_height; i <= height; ++i) {
        for (int j = region_width; j <= width; ++j) {
            // 计算当前区域的和
            int region_sum = cumulative_sum[i][j] - cumulative_sum[i - region_height][j]
                - cumulative_sum[i][j - region_width] + cumulative_sum[i - region_height][j - region_width];

            // 判断是否满足条件
            if (region_sum >= threshold_) { frame->frame_info->frame_event_result = 1; }
        }
    }

    output_image_(frame);
}

}// namespace nodes
}// namespace gddi