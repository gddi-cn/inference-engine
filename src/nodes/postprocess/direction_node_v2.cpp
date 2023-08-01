#include "direction_node_v2.h"
#include <algorithm>
#include <numeric>

#define M_PI 3.14159265358979323846 /* pi */

namespace gddi {
namespace nodes {

static float calc_distance(Point2f &point1, Point2f &point2) {
    return sqrt(pow((point2.x - point1.x), 2) + pow((point2.y - point1.y), 2));
}

static float alc_angle(Point2f &point1, Point2f &point2) {
    float x = point2.x - point1.x;
    float y = point2.y - point1.y;

    if (x == 0) { return y > 0 ? 270 : 90; }
    if (y == 0) { return x > 0 ? 0 : 180; }

    return fmod(360 - atan2(y, x) * 180 / M_PI, 360);
}

void Direction_v2::on_setup() { prev_timestamp_ = time(NULL); }

void Direction_v2::on_cv_frame(const std::shared_ptr<msgs::cv_frame> &frame) {
    frame->frame_info->ext_info.back().map_target_box.clear();

    for (const auto &[track_id, item] : frame->frame_info->ext_info.back().tracked_box) {
        Point2f cur_point{item.box.x + item.box.width / 2, item.box.y + item.box.height / 2};

        if (track_direction_[track_id].empty()) {
            track_direction_[track_id].emplace_back(TrackPoint{cur_point, 0, 0});
        } else {
            auto &last_point = track_direction_[track_id].back().point;

            // 记录与前一个框的间距和角度
            track_direction_[track_id].emplace_back(
                TrackPoint{cur_point, calc_distance(last_point, cur_point), alc_angle(last_point, cur_point)});

            // 计算周期
            if (time(NULL) - prev_timestamp_ >= duration_) {
                auto dis_accum = std::accumulate(
                    track_direction_[track_id].begin(), track_direction_[track_id].end(),
                    track_direction_[track_id][0].offset_distance
                        * cos((track_direction_[track_id][0].offset_angle - direction_) / 180 * M_PI),
                    [this](float dis_accum, const TrackPoint &object) {
                        return dis_accum
                            + object.offset_distance * cos((object.offset_angle - direction_) / 180 * M_PI);
                    });
                if (dis_accum / sqrt(frame->frame_info->area()) > distance_thresh_) {
                    track_direction_[track_id].clear();
                    frame->frame_info->ext_info.back()
                        .map_target_box[(int)frame->frame_info->ext_info.back().map_target_box.size()] =
                        {.prev_id = 0, .class_id = item.class_id, .prob = item.prob, .box = item.box};
                }

                // 更新时间戳
                prev_timestamp_ = time(NULL);
            }
        }
    }

    if (!frame->frame_info->ext_info.back().map_target_box.empty()) {
        frame->check_report_callback_ = [callback =
                                             frame->check_report_callback_](const std::vector<FrameExtInfo> &ext_info) {
            return std::max(FrameType::kReport, callback(ext_info));
        };
    }

    output_result_(frame);
}

}// namespace nodes
}// namespace gddi