#include "report_node_v2.h"
#include "modules/codec/encode_video_v3.h"
#include "modules/codec/remux_stream_v3.h"
#include "modules/network/event_socket.h"
#include "modules/network/zmq_socket.h"
#include "node_msg_def.h"
#include "wrapper/draw_image.h"
#include "wrapper/tsing_jpeg_encode.h"
#include <boost/filesystem.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#if defined(WITH_NVIDIA)
#define DEVICE_TYPE AV_HWDEVICE_TYPE_CUDA
#elif defined(WITH_MLU220) || defined(WITH_MLU270)
#define DEVICE_TYPE AV_HWDEVICE_TYPE_MLU
#else
#define DEVICE_TYPE AV_HWDEVICE_TYPE_NONE
#endif

namespace gddi {
namespace nodes {

const int skeleton_17[][2] = {{15, 13}, {13, 11}, {16, 14}, {14, 12}, {11, 12}, {5, 11}, {6, 12},
                              {5, 6},   {5, 7},   {6, 8},   {7, 9},   {8, 10},  {1, 2},  {0, 1},
                              {0, 2},   {1, 3},   {2, 4},   {3, 5},   {4, 6}};
const int skeleton_5[][2] = {{0, 1}, {0, 2}, {1, 2}, {0, 3}, {1, 4}, {2, 3}, {2, 4}, {3, 4}};

inline void create_directories(const std::string &path) { mkdir(path.c_str(), 0755); }

void Report_v2::on_setup() {
    last_event_time_ = time(NULL) - time_interval_;
    event_folder_ = "/home/data/raw_image/" + task_name_;
    create_directories("/home/data/raw_image/");
    create_directories("/home/data/raw_image/" + task_name_);
    draw_image_ = std::make_unique<DrawImage>();
    draw_image_->init_drawing("/home/config/NotoSansCJK-Regular.ttc");
}

void Report_v2::on_report(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (!jpeg_decoder_) {
        jpeg_decoder_ = std::make_unique<codec::TsingJpegEncode>();
        jpeg_decoder_->init_codecer(frame->frame_info->width(), frame->frame_info->height());
    }

    if (frame->task_type == TaskType::kCamera) {
        if (time(NULL) - last_event_time_ < time_interval_) { return; }
        last_event_time_ = time(NULL);

        if (real_time_push_) {
            auto topic = frame->task_name;
            auto pos = frame->task_name.find('_');
            if (pos != frame->task_name.npos) { topic = frame->task_name.substr(0, pos); }
            topic = topic + "_Report_v2";

            auto event_id = boost::uuids::to_string(boost::uuids::random_generator()());
            auto event_time = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now())
                                  .time_since_epoch()
                                  .count();
            auto buffer = frame_info_to_string(frame->task_name, event_id, event_time, frame->frame_info);
            network::ZmqSocket::get_instance("tcp://*:9000").send(topic, buffer);
        } else {
            // 检查上报条件
            if (frame->check_report_callback_(frame->frame_info->ext_info) < FrameType::kReport) {
                if (codec_type_ == "h264" || codec_type_ == "hevc") {
                    cache_frames_.push_back(frame->frame_info);
                    if (cache_frames_.size() > frame->infer_frame_rate * save_time_) {
                        cache_frames_.erase(cache_frames_.begin());
                    }
                }
                return;
            }

            auto event_id = boost::uuids::to_string(boost::uuids::random_generator()());

            if (codec_type_ == "h264" || codec_type_ == "hevc") {
                if (async_result_.valid()) {
                    if (async_result_.wait_for(std::chrono::milliseconds(40)) != std::future_status::ready) { return; }
                    async_result_.get();
                }
                async_result_ = std::async(std::launch::async, [this, frame, event_id]() {
                    export_video_ = std::make_unique<ExportVideo>();
                    export_video_->init_video(event_folder_ + "/" + event_id + ".mp4", frame->infer_frame_rate,
                                              frame->frame_info->width(), frame->frame_info->height());
                    auto cache_frames = this->cache_frames_;
                    auto iter = cache_frames.begin();
                    while (iter != cache_frames.end()) {
                        draw_image_->draw_frame(*iter);
                        export_video_->write_frame(*iter);
                        cache_frames.erase(iter);
                    }
                    export_video_->close_video();

                    jpeg_decoder_->save_image(frame->frame_info->src_frame->data,
                                              (event_folder_ + "/" + event_id + ".jpg").c_str());

                    auto buffer = frame_info_to_string(frame->task_name, event_id, time(NULL), frame->frame_info);
                    network::EventSocket::get_instance().post_sync(frame->task_name, event_id, report_url_, buffer);

                    spdlog::info("export video: {}", event_folder_ + "/" + event_id + ".mp4");
                });
            } else {
                jpeg_decoder_->save_image(frame->frame_info->src_frame->data,
                                          (event_folder_ + "/" + event_id + ".jpg").c_str());
                auto buffer = frame_info_to_string(frame->task_name, event_id, time(NULL), frame->frame_info);
                network::EventSocket::get_instance().post_sync(frame->task_name, event_id, report_url_, buffer);
            }
        }
    } else if (frame->task_type == TaskType::kVideo) {
        if (frame->frame_type == FrameType::kNone) {
            if (export_video_) { export_video_->close_video(); }

            spdlog::info("Predict Finish, Save at {}", report_url_);
            quit_runner_(TaskErrorCode::kNormal);
        } else {
            if (!export_video_) {
                create_directories(report_url_.substr(0, report_url_.find_last_of('/')));
                export_video_ = std::make_unique<ExportVideo>();
                export_video_->init_video(report_url_, frame->infer_frame_rate, frame->frame_info->width(),
                                          frame->frame_info->height());
            }
            draw_image_->draw_frame(frame->frame_info);
            export_video_->write_frame(frame->frame_info);
        }
    } else if (frame->task_type == TaskType::kImage) {
        draw_image_->draw_frame(frame->frame_info);
        create_directories(report_url_.substr(0, report_url_.find_last_of('/')));
        image_wrapper::image_save_as_jpeg(frame->frame_info->tgt_frame, report_url_, image_quality_);
        spdlog::info("Predict Finish, Save at {}", report_url_);
        quit_runner_(TaskErrorCode::kNormal);
    }
}

std::string Report_v2::frame_info_to_string(const std::string &task_name, const std::string &event_id,
                                            const int64_t &event_time,
                                            const std::shared_ptr<nodes::FrameInfo> &frame_info) {
    auto event_obj = nlohmann::json::object();
    event_obj["event_id"] = event_id;
    event_obj["task_id"] = task_name;
    event_obj["created"] = event_time;
    event_obj["details"] = nlohmann::json::array();

    const auto &last_ext_info = frame_info->ext_info.back();

    auto model_obj = nlohmann::json::object();
    model_obj["frame_id"] = frame_info->video_frame_idx;
    model_obj["model_id"] = last_ext_info.mod_id;
    model_obj["model_name"] = last_ext_info.mod_name;
    model_obj["model_type"] = last_ext_info.algo_type;
    model_obj["model_thres"] = last_ext_info.mod_thres;
    model_obj["regions"] = nlohmann::json::array();
    model_obj["regions_with_label"] = nlohmann::json::object();
    model_obj["targets"] = nlohmann::json::array();

    for (auto &[key, points] : frame_info->roi_points) {
        auto point_array = nlohmann::json::array();
        for (auto &point : points) { point_array.push_back({point.x, point.y}); }
        model_obj["regions_with_label"][key] = point_array;
        model_obj["regions"].emplace_back(point_array);
    }

    if (last_ext_info.algo_type == AlgoType::kSegmentation) {
        auto target_obj = nlohmann::json::object();
        target_obj["seg_info"] = nlohmann::json::array();
        for (const auto &[label, contours] : last_ext_info.seg_contours) {
            for (const auto &value : contours) {
                auto seg_obj = nlohmann::json::object();
                seg_obj["label"] = last_ext_info.map_class_label.at(label);
                seg_obj["length"] = value.area;
                seg_obj["area"] = value.length;
                seg_obj["volume"] = value.volume;
                target_obj["seg_info"].push_back(seg_obj);
            }
        }
        model_obj["targets"].push_back(target_obj);
    } else {
        for (const auto &[_, target] : last_ext_info.map_target_box) {
            auto target_obj = nlohmann::json::object();
            target_obj["id"] = target.track_id;
            target_obj["prev_id"] = target.prev_id;

            target_obj["label"] = last_ext_info.map_class_label.at(target.class_id);
            target_obj["prob"] = int(target.prob * 10000 + 0.5) / 10000.0;
            target_obj["moving"] = target.moving;

            // ROI 区域编号
            if (!target.roi_id.empty() && std::all_of(target.roi_id.begin(), target.roi_id.end(), [](unsigned char c) {
                    return std::isdigit(c);
                })) {
                target_obj["roi_id"] = std::stoi(target.roi_id);
            } else {
                target_obj["roi_id"] = 0;
            }
            target_obj["region_label"] = target.roi_id;

            // 目标测距
            target_obj["distance"] = target.distance;

            target_obj["box"]["left_top_x"] = target.box.x;
            target_obj["box"]["left_top_y"] = target.box.y;
            target_obj["box"]["right_bottom_x"] = target.box.x + target.box.width;
            target_obj["box"]["right_bottom_y"] = target.box.y + target.box.height;
            target_obj["color"] = last_ext_info.map_class_color.at(target.class_id).to_array<int>();

            if (last_ext_info.algo_type == AlgoType::kPose || last_ext_info.algo_type == AlgoType::kAction) {
                target_obj["key_points"] = nlohmann::json::array();
                auto &key_points = frame_info->ext_info.front().map_key_points.at(target.prev_id);
                for (const auto &point : key_points) {
                    auto key_point_obj = nlohmann::json::object();
                    key_point_obj["x"] = point.x;
                    key_point_obj["y"] = point.y;
                    key_point_obj["prob"] = point.prob;
                    target_obj["key_points"].push_back(key_point_obj);
                }

                if (key_points.size() == 17) {
                    for (const auto &bone : skeleton_17) {
                        std::array<int, 4> color;
                        if (bone[0] < 5 || bone[1] < 5) color = {0, 255, 0, 1};
                        else if (bone[0] > 12 || bone[1] > 12)
                            color = {255, 0, 0, 1};
                        else if (bone[0] > 4 && bone[0] < 11 && bone[1] > 4 && bone[1] < 11)
                            color = {0, 255, 255, 1};
                        else
                            color = {255, 0, 255, 1};

                        auto point_obj = nlohmann::json::object();
                        point_obj["start_x"] = key_points[bone[0]].x;
                        point_obj["start_y"] = key_points[bone[0]].y;
                        point_obj["end_x"] = key_points[bone[1]].x;
                        point_obj["end_y"] = key_points[bone[1]].y;
                        point_obj["color"] = color;
                        target_obj["key_lines"].push_back(point_obj);
                    }
                } else if (key_points.size() == 5) {
                    for (const auto &bone : skeleton_5) {
                        auto point_obj = nlohmann::json::object();
                        point_obj["start_x"] = key_points[bone[0]].x;
                        point_obj["start_y"] = key_points[bone[0]].y;
                        point_obj["end_x"] = key_points[bone[1]].x;
                        point_obj["end_y"] = key_points[bone[1]].y;
                        point_obj["color"] = Scalar({0, 255, 0, 1}).to_array();
                        target_obj["key_lines"].push_back(point_obj);
                    }
                }
            }

            for (const auto &item : last_ext_info.mosaic_rects) {
                auto mosaic_obj = nlohmann::json::object();
                mosaic_obj["left_top_x"] = item.x;
                mosaic_obj["left_top_y"] = item.y;
                mosaic_obj["right_bottom_x"] = item.x + item.width;
                mosaic_obj["right_bottom_y"] = item.y + item.height;
                mosaic_obj["color"] = {255, 255, 255, 1};
                model_obj["mosaics"].emplace_back(std::move(mosaic_obj));
            }

            model_obj["targets"].push_back(target_obj);
        }

        // 越线计数
        for (const auto &item : last_ext_info.cross_count) {
            std::map<std::string, std::array<int, 2>> label_count;
            for (const auto &[key, values] : item) {
                label_count[key][0] += values[0];
                label_count[key][1] += values[1];
            }
            model_obj["cross_info"].emplace_back(label_count);
        }

        /*************************** 越界计数可视化处理 **************************/
        int len = last_ext_info.border_points.size();
        for (int i = 0; i < len; i++) {
            auto &points = last_ext_info.border_points[i];
            auto target_obj = nlohmann::json::object();
            target_obj["key_points"] = nlohmann::json::array();
            int len = points.size();
            for (int i = 0; i < len - 1; i++) {
                auto x1 = points[i].x;
                auto y1 = points[i].y;
                auto x2 = points[i + 1].x;
                auto y2 = points[i + 1].y;
                auto point_obj = nlohmann::json::object();
                point_obj["start_x"] = x1;
                point_obj["start_y"] = y1;
                point_obj["end_x"] = x2;
                point_obj["end_y"] = y2;
                point_obj["color"] = std::array<int, 4>{255, 0, 0, 1};
                target_obj["key_lines"].emplace_back(point_obj);
            }

            int index = 0;
            for (auto &[key, values] : last_ext_info.cross_count[i]) {
                for (const auto &value : values) {
                    target_obj["box"]["left_top_x"] = (points[0].x + points[1].x) / 2;
                    target_obj["box"]["left_top_y"] = (points[0].y + points[1].y) / 2 + index * 30;
                    target_obj["box"]["right_bottom_x"] = (points[0].x + points[1].x) / 2;
                    target_obj["box"]["right_bottom_y"] = (points[0].y + points[1].y) / 2 + index * 30;
                    target_obj["label"] = key;
                    target_obj["prob"] = value;
                    target_obj["color"] = std::array<int, 4>{255, 0, 0, 1};
                    model_obj["targets"].emplace_back(target_obj);
                    ++index;
                }
            }
        }
        /***********************************************************************/

        /****************************** 目标计数可视化 *****************************/
        int index = 0;
        for (const auto &[key, value] : last_ext_info.target_counts) {
            auto target_obj = nlohmann::json::object();
            model_obj["stat_info"][key] = value;
            target_obj["box"]["left_top_x"] = 0;
            target_obj["box"]["left_top_y"] = index * 80;
            target_obj["box"]["right_bottom_x"] = 0;
            target_obj["box"]["right_bottom_y"] = index * 80;
            target_obj["prob"] = value;
            target_obj["label"] = key;
            target_obj["color"] = std::array<int, 4>{255, 0, 0, 1};
            model_obj["targets"].push_back(target_obj);
            ++index;
        }
        /***********************************************************************/

        /******************************** OCR 可视化 ****************************/
        for (const auto &[track_id, ocr_info] : last_ext_info.map_ocr_info) {
            auto box_obj = nlohmann::json::object();
            std::vector<cv::Point2i> points;
            for (const auto &point : ocr_info.points) { points.emplace_back(point.x, point.y); }
            auto rect = cv::boundingRect(points);

            auto target_obj = nlohmann::json::object();
            target_obj["id"] = track_id;
            target_obj["prev_id"] = track_id;

            std::string label;
            for (auto &value : ocr_info.labels) { label += value.str; }
            target_obj["label"] = label;
            target_obj["prob"] = 0;
            target_obj["roi_id"] = 0;
            target_obj["distance"] = 0;

            target_obj["box"]["left_top_x"] = rect.x;
            target_obj["box"]["left_top_y"] = rect.y;
            target_obj["box"]["right_bottom_x"] = rect.x + rect.width;
            target_obj["box"]["right_bottom_y"] = rect.y + rect.height;
            target_obj["color"] = std::array<int, 4>{255, 0, 0, 1};

            model_obj["targets"].push_back(target_obj);
        }
        /***********************************************************************/
    }

    event_obj["details"].push_back(model_obj);

    return event_obj.dump();
}

}// namespace nodes
}// namespace gddi
