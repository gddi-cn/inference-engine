#include "inference_node_v2.h"
#include "algorithm/abstract_algo.h"
#include "common_basic/thread_dbg_utils.hpp"
#include "node_struct_def.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <spdlog/spdlog.h>
#include <utility>

namespace gddi {
namespace nodes {

void Inference_v2::on_setup() {
    for (size_t i = 0; i < parms_.instances; i++) {
        thread_handle_.emplace_back(std::thread([this, i]() {
            gddi::thread_utils::set_cur_thread_name(std::string("inference-r:" + std::to_string(i)));

            std::mutex infer_mtx;
            std::map<int64_t, std::shared_ptr<msgs::cv_frame>> infer_frames;

            auto algo_impl = algo::make_algo_impl();

            algo_impl->register_init_callback([this](const std::vector<std::string> &vec_labels) {
                try {
                    int index = 0;
                    if (mod_label_objs_.count("all")) {
                        for (auto &item : vec_labels) {
                            map_class_label_[index] = item;
                            map_class_color_[index] = {0, 0, 255, int(255 * 0.8)};
                            index++;
                        }
                    } else {
                        for (auto &item : vec_labels) {
                            if (mod_label_objs_.count(item) > 0 && mod_label_objs_[item]["checked"].get<bool>()) {
                                map_class_label_[index] = mod_label_objs_[item]["label"].get<std::string>();
                                auto color = mod_label_objs_[item]["color"].get<std::vector<int>>();
                                map_class_color_[index] = {color[0], color[1], color[2], int(255 * 0.8)};
                            }
                            index++;
                        }
                    }
                } catch (const std::exception &e) {
                    spdlog::error(e.what());
                    quit_runner_(TaskErrorCode::kInference);
                }
            });

            algo_impl->register_infer_callback(
                [this, &infer_mtx, &infer_frames](const int64_t frame_idx, const AlgoType type,
                                                  const std::vector<algo::AlgoOutput> &vec_output) {
                    std::shared_ptr<msgs::cv_frame> frame;
                    {
                        std::lock_guard<std::mutex> lck(infer_mtx);
                        frame = infer_frames.at(frame_idx);
                        infer_frames.erase(frame_idx);
                    }

                    frame->frame_info->ext_info.emplace_back(parser_output(type, vec_output));
                    output_result_(frame);
                });

            auto future = std::async(std::launch::async, [&algo_impl, this]() { return algo_impl->init(parms_); });

            std::shared_ptr<msgs::cv_frame> frame;
            while (active) {
                if (cache_frames_.wait_dequeue_timed(frame, std::chrono::seconds(1))) {
                    if (frame->frame_type == FrameType::kNone) {
                        output_result_(frame);
                        continue;
                    }

                    // 模型加载处理
                    if (future.valid()) {
                        if (frame->task_type == TaskType::kImage || frame->task_type == TaskType::kAsyncImage
                            || frame->task_type == TaskType::kVideo) {
                            future.get();
                        } else if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                            if (!future.get()) {
                                // 模型加载失败
                                quit_runner_(TaskErrorCode::kInference);
                                break;
                            }
                        } else {
                            // 模型加载未完成
                            frame->frame_info->ext_info.emplace_back(parser_output(AlgoType::kUndefined, {}));
                            output_result_(frame);
                            continue;
                        }
                    }

                    if (frame->infer_frame_rate < parms_.frame_rate
                        || fmod(frame->frame_info->infer_frame_idx, frame->infer_frame_rate * 1.0 / parms_.frame_rate)
                            < 1) {
                        frame->infer_frame_rate = std::min(frame->infer_frame_rate, parms_.frame_rate);
                        frame->frame_info->infer_frame_idx = infer_frame_idx_++;
                        if (frame->frame_info->ext_info.empty()) {

                            {
                                std::lock_guard<std::mutex> lck(infer_mtx);
                                infer_frames.insert(std::make_pair(frame->frame_info->infer_frame_idx, frame));
                            }

                            // 一阶段
                            if (frame->task_type == TaskType::kImage || frame->task_type == TaskType::kAsyncImage) {
                                algo_impl->inference(frame->task_name, frame->frame_info, algo::InferType::kSync);
                            } else {
                                algo_impl->inference(frame->task_name, frame->frame_info, algo::InferType::kAsync);
                            }
                        } else {
                            // 多阶段
                            std::map<int, std::vector<algo::AlgoOutput>> outputs;
                            auto algo_type = algo_impl->inference(frame->task_name, frame->frame_info, outputs);
                            frame->frame_info->ext_info.emplace_back(
                                parser_output(algo_type, frame->frame_info->ext_info.back(), outputs));
                            output_result_(frame);
                        }
                    }
                }
            }
        }));
    }
}

Inference_v2::~Inference_v2() {
    active = false;
    for (auto &handle : thread_handle_) {
        if (handle.joinable()) { handle.join(); }
    }
}

void Inference_v2::on_cv_frame(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (cache_frames_.size_approx() > 20) {
        spdlog::info("Inference_v2 cache_frames_size: {}", cache_frames_.size_approx());
        return;
    }

    if (frame->task_type == TaskType::kAsyncImage || frame->frame_type == FrameType::kNone) {
        cache_frames_.enqueue(frame);
    } else {
        cache_frames_.enqueue(std::make_shared<msgs::cv_frame>(frame));
    }
}

nodes::FrameExtInfo Inference_v2::parser_output(const AlgoType type, const std::vector<algo::AlgoOutput> &vec_output) {
    nodes::FrameExtInfo ext_info(type, parms_.mod_id, parms_.mod_name, parms_.mod_thres);
    ext_info.map_class_label = map_class_label_;
    for (auto &item : map_class_color_) {
        ext_info.map_class_color[item.first] = {(float)item.second[0], (float)item.second[1], (float)item.second[2],
                                                (float)item.second[3]};
    }

    int index = 0;
    for (const auto &item : vec_output) {
        // 置信度小于设置阈值
        if (item.prob < parms_.mod_thres) { continue; }

        // 已勾选类别
        if (map_class_label_.count(item.class_id) > 0) {
            if (type == AlgoType::kDetection || type == AlgoType::kClassification) {
                ext_info.map_target_box.insert(std::make_pair(
                    index, BoxInfo{.prev_id = index, .class_id = item.class_id, .prob = item.prob, .box = item.box}));
            } else if (type == AlgoType::kPose) {
                std::vector<PoseKeyPoint> vec_key_points;
                ext_info.map_target_box.insert(std::make_pair(
                    index, BoxInfo{.prev_id = index, .class_id = item.class_id, .prob = item.prob, .box = item.box}));
                for (auto &key_points : item.vec_key_points) {
                    vec_key_points.push_back(nodes::PoseKeyPoint{.number = std::get<0>(key_points),
                                                                 .x = std::get<1>(key_points),
                                                                 .y = std::get<2>(key_points),
                                                                 .prob = std::get<3>(key_points)});
                }
                ext_info.map_key_points.insert(std::make_pair(index, vec_key_points));
            } else if (type == AlgoType::kSegmentation) {
                ext_info.seg_width = item.seg_width;
                ext_info.seg_height = item.seg_height;
                ext_info.seg_map = std::move(item.seg_map);
            } else if (type == AlgoType::kOCR_DET) {
                for (auto &ocr_info : item.vec_ocr_info) {
                    ext_info.map_ocr_info.insert(std::make_pair(index, ocr_info));
                }
            }
            ++index;
        }
    }
    ext_info.infer_target_info = ext_info.map_target_box;

    return ext_info;
}

nodes::FrameExtInfo Inference_v2::parser_output(const AlgoType type, const FrameExtInfo &back_ext_info,
                                                const std::map<int, std::vector<algo::AlgoOutput>> &outputs) {
    nodes::FrameExtInfo ext_info(type, parms_.mod_id, parms_.mod_name, parms_.mod_thres);
    ext_info.map_class_label = map_class_label_;
    for (auto &item : map_class_color_) {
        ext_info.map_class_color[item.first] = {(float)item.second[0], (float)item.second[1], (float)item.second[2], 1};
    }

    int index = 0;
    for (const auto &[prev_id, output] : outputs) {
        for (const auto &item : output) {
            // 置信度小于设置阈值
            if (item.prob < parms_.mod_thres) { continue; }

            if (type == AlgoType::kOCR_REC) {
                for (auto ocr_info : item.vec_ocr_info) {
                    ocr_info.roi_id = back_ext_info.map_target_box.at(prev_id).roi_id;
                    ext_info.map_ocr_info[prev_id] = ocr_info;
                }
            } else if (type == AlgoType::kOCR) {
                auto &prev_target = back_ext_info.map_target_box.at(prev_id);

                std::vector<PoseKeyPoint> pose_key_point;
                pose_key_point.emplace_back(PoseKeyPoint{0, prev_target.box.x, prev_target.box.y, 1});
                pose_key_point.emplace_back(PoseKeyPoint{1, prev_target.box.x + prev_target.box.width,
                                                         prev_target.box.y + prev_target.box.height, 1});
                std::vector<LableInfo> label_info{{.class_id = 0, .prob = prev_target.prob, .str = item.ocr_str}};
                ext_info.map_ocr_info.insert(
                    std::make_pair(prev_id, gddi::nodes::OcrInfo{.points = pose_key_point, .labels = label_info}));
            } else if (back_ext_info.flag_crop) {
                auto &prev_target = back_ext_info.map_target_box.at(prev_id);

                if (map_class_label_.count(item.class_id) > 0) {
                    // 已勾选类别 && 置信度大于模型阈值
                    if (type == AlgoType::kDetection) {
                        ext_info.map_target_box.insert(std::make_pair(index,
                                                                      BoxInfo{.prev_id = prev_id,
                                                                              .class_id = item.class_id,
                                                                              .prob = item.prob,
                                                                              .box = item.box,
                                                                              .roi_id = prev_target.roi_id,
                                                                              .track_id = prev_target.track_id}));
                    } else if (type == AlgoType::kClassification) {
                        ext_info.map_target_box.insert(std::make_pair(index,
                                                                      BoxInfo{.prev_id = prev_id,
                                                                              .class_id = item.class_id,
                                                                              .prob = item.prob,
                                                                              .box = prev_target.box,
                                                                              .roi_id = prev_target.roi_id,
                                                                              .track_id = prev_target.track_id}));
                    } else if (type == AlgoType::kPose) {
                        std::vector<PoseKeyPoint> vec_key_points;
                        ext_info.map_target_box.insert(std::make_pair(index,
                                                                      BoxInfo{.prev_id = prev_id,
                                                                              .class_id = item.class_id,
                                                                              .prob = item.prob,
                                                                              .box = item.box,
                                                                              .roi_id = prev_target.roi_id,
                                                                              .track_id = prev_target.track_id}));
                        for (auto &key_points : item.vec_key_points) {
                            vec_key_points.push_back(nodes::PoseKeyPoint{.number = std::get<0>(key_points),
                                                                         .x = std::get<1>(key_points),
                                                                         .y = std::get<2>(key_points),
                                                                         .prob = std::get<3>(key_points)});
                        }
                        ext_info.map_key_points.insert(std::make_pair(index, vec_key_points));
                    } else if (type == AlgoType::kSegmentation) {
                        ext_info.seg_map = std::move(item.seg_map);
                    }
                } else if (type == AlgoType::kOCR) {
                    std::vector<PoseKeyPoint> pose_key_point;
                    pose_key_point.emplace_back(PoseKeyPoint{0, prev_target.box.x, prev_target.box.y, 1});
                    pose_key_point.emplace_back(PoseKeyPoint{1, prev_target.box.x + prev_target.box.width,
                                                             prev_target.box.y + prev_target.box.height, 1});
                    std::vector<LableInfo> label_info{{.class_id = 0, .prob = prev_target.prob, .str = item.ocr_str}};
                    ext_info.map_ocr_info.insert(
                        std::make_pair(prev_id, gddi::nodes::OcrInfo{.points = pose_key_point, .labels = label_info}));
                }

                if (ext_info.map_target_box.count(index) > 0) {
                    ext_info.tracked_box[prev_target.track_id] =
                        TrackInfo{.target_id = index,
                                  .class_id = ext_info.map_target_box[index].class_id,
                                  .prob = ext_info.map_target_box[index].prob,
                                  .box = ext_info.map_target_box[index].box};
                }
            } else if (map_class_label_.count(item.class_id) > 0) {
                if (type == AlgoType::kDetection) {
                    ext_info.map_target_box.insert(std::make_pair(
                        index,
                        BoxInfo{.prev_id = index, .class_id = item.class_id, .prob = item.prob, .box = item.box}));
                    ext_info.tracked_box[index] =
                        TrackInfo{.target_id = index, .class_id = item.class_id, .prob = item.prob, .box = item.box};
                } else if (type == AlgoType::kClassification) {
                    ext_info.map_target_box.insert(std::make_pair(index,
                                                                  BoxInfo{.prev_id = index,
                                                                          .class_id = item.class_id,
                                                                          .prob = item.prob,
                                                                          .box = ext_info.map_target_box[index].box}));
                    ext_info.tracked_box[index] = TrackInfo{.target_id = index,
                                                            .class_id = item.class_id,
                                                            .prob = item.prob,
                                                            .box = ext_info.map_target_box[index].box};
                } else if (type == AlgoType::kPose) {
                    std::vector<PoseKeyPoint> vec_key_points;
                    ext_info.map_target_box.insert(std::make_pair(
                        index,
                        BoxInfo{.prev_id = index, .class_id = item.class_id, .prob = item.prob, .box = item.box}));
                    for (auto &key_points : item.vec_key_points) {
                        vec_key_points.push_back(nodes::PoseKeyPoint{.number = std::get<0>(key_points),
                                                                     .x = std::get<1>(key_points),
                                                                     .y = std::get<2>(key_points),
                                                                     .prob = std::get<3>(key_points)});
                    }
                    ext_info.map_key_points.insert(std::make_pair(index, vec_key_points));
                } else if (type == AlgoType::kSegmentation) {
                    ext_info.seg_width = item.seg_width;
                    ext_info.seg_height = item.seg_height;
                    ext_info.seg_map = std::move(item.seg_map);
                } else if (type == AlgoType::kOCR_DET) {
                    for (auto &ocr_info : item.vec_ocr_info) {
                        ext_info.map_ocr_info.insert(std::make_pair(index, ocr_info));
                    }
                } else if (type == AlgoType::kAction) {
                    auto &prev_target = back_ext_info.map_target_box.at(prev_id);
                    ext_info.cur_action_score[prev_target.track_id] = std::make_pair(item.class_id, item.prob);
                    ext_info.tracked_box[prev_target.track_id] = TrackInfo{.target_id = index,
                                                                           .class_id = item.class_id,
                                                                           .prob = item.prob,
                                                                           .box = prev_target.box};
                }
            } else if (type == AlgoType::kFace) {
                auto &prev_target = back_ext_info.map_target_box.at(prev_id);
                ext_info.map_target_box.insert(std::make_pair(index,
                                                              BoxInfo{.prev_id = prev_id,
                                                                      .class_id = item.class_id,
                                                                      .prob = item.prob,
                                                                      .box = prev_target.box,
                                                                      .roi_id = prev_target.roi_id,
                                                                      .track_id = prev_target.track_id}));
                ext_info.map_key_points[index] = back_ext_info.map_key_points.at(prev_id);
                ext_info.features[prev_target.track_id] = std::move(item.feature);
                ext_info.map_class_label[item.class_id] = item.label;
                ext_info.map_class_color[item.class_id] = Scalar{0, 0, 255, 1};
                ext_info.tracked_box[prev_target.track_id] =
                    TrackInfo{.target_id = index, .class_id = item.class_id, .prob = item.prob, .box = prev_target.box};
            }

            ++index;
        }
    }

    for (const auto &[track_id, item] : back_ext_info.tracked_box) {
        auto &prev_target = back_ext_info.map_target_box.at(item.target_id);
        if (ext_info.tracked_box.count(track_id) == 0) {
            if (type == AlgoType::kAction) {
                ext_info.map_target_box.insert(std::make_pair(index,
                                                              BoxInfo{.prev_id = item.target_id,
                                                                      .class_id = map_class_label_.begin()->first,
                                                                      .prob = item.prob,
                                                                      .box = prev_target.box,
                                                                      .roi_id = prev_target.roi_id,
                                                                      .track_id = track_id}));
                ext_info.map_key_points[track_id] = back_ext_info.map_key_points.at(item.target_id);
                ext_info.tracked_box[track_id] = TrackInfo{.target_id = index,
                                                           .class_id = map_class_label_.begin()->first,
                                                           .prob = 0,
                                                           .box = ext_info.map_target_box[index].box};
            }
        }
        ++index;
    }

    return ext_info;
}

}// namespace nodes
}// namespace gddi