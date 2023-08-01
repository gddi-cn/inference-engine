#include "target_tracker_node_v2.h"
#include "node_msg_def.h"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace gddi {
namespace nodes {
void TargetTracker_v2::on_setup() {}

void TargetTracker_v2::on_cv_frame(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == FrameType::kNone) {
        output_result_(frame);
        return;
    }

    if (!tracking_) {
        tracking_ = std::make_shared<TargetTracker>(
            TrackOption{.track_thresh = frame->frame_info->ext_info.back().mod_thres,
                        .high_thresh = 0.6,
                        .match_thresh = 0.8,
                        .max_frame_lost = int(max_lost_time_ * frame->infer_frame_rate)});
    }

    auto &front_ext_info = frame->frame_info->ext_info.front();
    auto &back_ext_info = frame->frame_info->ext_info.back();
    std::vector<TrackObject> vec_objects;

    if (frame->frame_info->ext_info.size() > 1) {
        // 用于兼容之前二阶段模板
        back_ext_info.tracked_box.clear();
        for (const auto &[idx, item] : front_ext_info.map_target_box) {
            vec_objects.push_back(TrackObject{.target_id = idx,
                                              .class_id = item.class_id,
                                              .prob = item.prob,
                                              .rect = {item.box.x, item.box.y, item.box.width, item.box.height}});
        }
    } else {
        for (const auto &[idx, item] : back_ext_info.map_target_box) {
            vec_objects.push_back(TrackObject{.target_id = idx,
                                              .class_id = item.class_id,
                                              .prob = item.prob,
                                              .rect = {item.box.x, item.box.y, item.box.width, item.box.height}});
        }
    }

    // for (const auto &[idx, item] : back_ext_info.map_ocr_info) {
    //     std::vector<cv::Point2f> points;
    //     for (const auto &point : item.points) { points.emplace_back(point.x, point.y); }
    //     auto rect = cv::boundingRect(points);
    //     vec_objects.push_back(
    //         TrackObject{.target_id = idx,
    //                     .class_id = 0,
    //                     .prob = 1,
    //                     .rect = {(float)rect.x, (float)rect.y, (float)rect.width, (float)rect.height}});
    // }

    auto tracked_target = tracking_->update_objects(vec_objects);
    for (auto &[track_id, item] : tracked_target) {
        if (frame->frame_info->ext_info.size() > 1) {
            // 用于兼容之前二阶段模板
            front_ext_info.tracked_box[track_id] =
                TrackInfo{.target_id = item.target_id, .class_id = item.class_id, .prob = item.prob, .box = item.rect};
            front_ext_info.map_target_box.at(item.target_id).track_id = track_id;

            auto iter = std::find_if(back_ext_info.map_target_box.begin(), back_ext_info.map_target_box.end(),
                                     [target_id = item.target_id](const std::pair<int, BoxInfo> &value) {
                                         return value.second.prev_id == target_id;
                                     });
            if (iter != back_ext_info.map_target_box.end()) {
                iter->second.track_id = track_id;
                back_ext_info.tracked_box[track_id] = TrackInfo{.target_id = iter->first,
                                                                .class_id = iter->second.class_id,
                                                                .prob = iter->second.prob,
                                                                .box = iter->second.box};
            }
        } else {
            back_ext_info.tracked_box[track_id] =
                TrackInfo{.target_id = item.target_id, .class_id = item.class_id, .prob = item.prob, .box = item.rect};
            back_ext_info.map_target_box.at(item.target_id).track_id = track_id;
        }
    }

    frame->check_report_callback_ = [that = std::reinterpret_pointer_cast<TargetTracker_v2>(shared_from_this()),
                                     callback =
                                         frame->check_report_callback_](const std::vector<FrameExtInfo> &ext_info) {
        FrameType result{FrameType::kBase};

        for (const auto &[track_id, item] : ext_info.back().tracked_box) {
            // 新目标出现
            if (that->target_status_.count(track_id) == 0) {
                that->target_status_[track_id].pre_class_id = -1;
                that->target_status_[track_id].prev_timestamp = time(NULL);
            }

            auto &cur_target = that->target_status_.at(track_id);
            cur_target.last_timestamp = time(NULL);
            cur_target.class_ids.emplace_back(item.class_id);

            // 目标出现上报，并且大于持续跟踪时间
            if (that->appear_report_
                && cur_target.last_timestamp - cur_target.prev_timestamp >= that->continuous_tracking_time_) {

                std::unordered_map<int, int> count_map;
                for (const auto &num : cur_target.class_ids) { ++count_map[num]; }

                auto max_element = std::max_element(count_map.begin(), count_map.end(),
                                                    [](const auto &a, const auto &b) { return a.second < b.second; });

                // 80% 以上的帧都是同一类别
                if (max_element->second * 1.0f / cur_target.class_ids.size() >= 0.8) {
                    // 上报目标类别变化，且当前类别与上一帧不同，且当前类别与上一次上报类别不同
                    if (cur_target.pre_class_id != max_element->first && max_element->first == item.class_id) {
                        cur_target.pre_class_id = item.class_id;
                        result = FrameType::kReport;
                    }

                    cur_target.prev_timestamp = time(NULL);
                    cur_target.class_ids.clear();
                } else {
                    cur_target.class_ids.erase(cur_target.class_ids.begin());
                }
            }
        }

        // 目标丢失
        for (auto iter = that->target_status_.begin(); iter != that->target_status_.end();) {
            if (time(NULL) - iter->second.last_timestamp > that->max_lost_time_) {
                spdlog::debug("Del track id: {}", iter->first);
                // 目标丢失上报
                if (that->disappear_report_
                    && iter->second.last_timestamp - iter->second.prev_timestamp >= that->continuous_tracking_time_) {
                    result = FrameType::kDisappear;
                }
                iter = that->target_status_.erase(iter);
            } else {
                ++iter;
            }
        }

        if (ext_info.back().algo_type == AlgoType::kSegmentation) { result = FrameType::kReport; }

        return std::max(result, callback(ext_info));
    };

    output_result_(frame);
}

}// namespace nodes
}// namespace gddi