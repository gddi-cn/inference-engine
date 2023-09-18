//
// Created by cc on 2021/12/1.
//

#ifndef __JPEG_PREVIEWER_V2__
#define __JPEG_PREVIEWER_V2__

#include "modules/network/zmq_socket.h"
#include "modules/wrapper/tsing_jpeg_encode.h"
#include "node_struct_def.h"
#include "nodes/node_any_basic.hpp"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <thread>
#include <vector>

namespace gddi {
namespace nodes {

const int skeleton[][2] = {{15, 13}, {13, 11}, {16, 14}, {14, 12}, {11, 12}, {5, 11}, {6, 12}, {5, 6}, {5, 7}, {6, 8},
                           {7, 9},   {8, 10},  {1, 2},   {0, 1},   {0, 2},   {1, 3},  {2, 4},  {3, 5}, {4, 6}};

const int skeleton_5[][2] = {{0, 1}, {0, 2}, {1, 2}, {0, 3}, {1, 4}, {2, 3}, {2, 4}, {3, 4}};

class JpegPreviewer_v2 : public node_any_basic<JpegPreviewer_v2> {
public:
    explicit JpegPreviewer_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("address", address_, "推流地址");
        bind_simple_property("quality", quality_, 0, 100, "预览质量");
        bind_simple_property("scale", scale_, 1, 16, "分屏预览数");
        bind_simple_property("masking", masking_, "背景脱敏");
        bind_simple_property("node_name", node_name_, "节点名称");

        register_input_message_handler_<msgs::cv_frame>([=](const std::shared_ptr<msgs::cv_frame> &frame) {
            if (get_spec_data().count("enable") == 0 || get_spec_data().at("enable") != "true") { return; }
            if (get_spec_data().count("scale") > 0) { scale_ = std::atoi(get_spec_data().at("scale").c_str()); }

            scale_ = 1;

            if (masking_) {
                // 每分钟更新一次背景
                if (frame->frame_info->ext_info.front().infer_target_info.empty()) {
                    if (!jpeg_encoder_) {
                        jpeg_encoder_ = std::make_unique<codec::TsingJpegEncode>();
                        if (!jpeg_encoder_->init_codecer(frame->frame_info->width(), frame->frame_info->height())) {
                            jpeg_encoder_.reset();
                            return;
                        }
                    }

                    jpeg_encoder_->codec_image(frame->frame_info->src_frame->data, jpeg_data_.data());
                }
            }

            auto topic = frame->task_name;
            auto pos = frame->task_name.find('_');
            if (pos != frame->task_name.npos) { topic = frame->task_name.substr(0, pos); }
            topic = topic + "_" + node_name_;

            auto json_obj = nlohmann::json::object();

            int count = 0;
            auto box_array = nlohmann::json::array();
            auto key_point_array = nlohmann::json::array();

            auto &back_ext_info = frame->frame_info->ext_info.back();

            for (const auto &[track_id, item] : back_ext_info.tracked_box) {
                auto box_obj = nlohmann::json::object();
                box_obj["left_top_x"] = item.box.x / scale_;
                box_obj["left_top_y"] = item.box.y / scale_;
                box_obj["right_bottom_x"] = (item.box.x + item.box.width) / scale_;
                box_obj["right_bottom_y"] = (item.box.y + item.box.height) / scale_;
                box_obj["prob"] = item.prob;
                box_obj["label"] = back_ext_info.map_class_label.at(item.class_id) + "-" + std::to_string(track_id);
                box_obj["color"] = back_ext_info.map_class_color.at(item.class_id).to_array();
                box_array.push_back(box_obj);

                if (back_ext_info.algo_type == AlgoType::kAction) {
                    auto box_obj = nlohmann::json::object();
                    box_obj["left_top_x"] = item.box.x / scale_;
                    box_obj["left_top_y"] = (item.box.y + 25) / scale_;
                    box_obj["right_bottom_x"] = item.box.x / scale_;
                    box_obj["right_bottom_y"] = (item.box.y + 25) / scale_;
                    if (back_ext_info.sum_action_scores.count(track_id) > 0) {
                        box_obj["prob"] = back_ext_info.sum_action_scores.at(track_id).begin()->second.size();
                    } else {
                        box_obj["prob"] = 0;
                    }
                    box_obj["label"] = "total";
                    box_obj["color"] = std::array<int, 4>{255, 0, 0, 0};
                    box_array.push_back(box_obj);
                }
            }

            count += back_ext_info.tracked_box.size();

            // 模板兼容性处理
            if (back_ext_info.tracked_box.size() == 0) {
                for (const auto &[idx, item] : back_ext_info.map_target_box) {
                    auto box_obj = nlohmann::json::object();
                    box_obj["left_top_x"] = item.box.x / scale_;
                    box_obj["left_top_y"] = item.box.y / scale_;
                    box_obj["right_bottom_x"] = (item.box.x + item.box.width) / scale_;
                    box_obj["right_bottom_y"] = (item.box.y + item.box.height) / scale_;
                    box_obj["prob"] = item.prob;
                    box_obj["label"] =
                        back_ext_info.map_class_label.at(item.class_id) + "-" + std::to_string(item.track_id);
                    box_obj["color"] = back_ext_info.map_class_color.at(item.class_id).to_array();
                    box_array.push_back(box_obj);
                }

                count += back_ext_info.map_target_box.size();
            }

            // 关键点
            for (const auto &[idx, key_points] : back_ext_info.map_key_points) {
                auto points_array = nlohmann::json::array();
                if (key_points.size() == 17) {
                    for (const auto &bone : skeleton) {
                        Scalar color;
                        if (bone[0] < 5 || bone[1] < 5) color = {0, 255, 0, 1};
                        else if (bone[0] > 12 || bone[1] > 12)
                            color = {255, 0, 0, 1};
                        else if (bone[0] > 4 && bone[0] < 11 && bone[1] > 4 && bone[1] < 11)
                            color = {0, 255, 255, 1};
                        else
                            color = {255, 0, 255, 1};

                        auto point_obj = nlohmann::json::object();
                        point_obj["start_x"] = key_points[bone[0]].x / scale_;
                        point_obj["start_y"] = key_points[bone[0]].y / scale_;
                        point_obj["end_x"] = key_points[bone[1]].x / scale_;
                        point_obj["end_y"] = key_points[bone[1]].y / scale_;
                        point_obj["color"] = color.to_array();
                        points_array.push_back(point_obj);
                    }
                } else if (key_points.size() == 5) {
                    for (const auto &bone : skeleton_5) {
                        auto point_obj = nlohmann::json::object();
                        point_obj["start_x"] = key_points[bone[0]].x / scale_;
                        point_obj["start_y"] = key_points[bone[0]].y / scale_;
                        point_obj["end_x"] = key_points[bone[1]].x / scale_;
                        point_obj["end_y"] = key_points[bone[1]].y / scale_;
                        point_obj["color"] = Scalar({0, 255, 0, 1}).to_array();
                        points_array.push_back(point_obj);
                    }
                } else if (key_points.size() == 2) {
                    auto point_obj = nlohmann::json::object();
                    point_obj["start_x"] = key_points[0].x / scale_;
                    point_obj["start_y"] = key_points[0].y / scale_;
                    point_obj["end_x"] = key_points[1].x / scale_;
                    point_obj["end_y"] = key_points[1].y / scale_;
                    point_obj["color"] = Scalar({0, 255, 0, 1}).to_array();
                    points_array.push_back(point_obj);
                }
                key_point_array.push_back(points_array);
            }

            for (const auto &item : back_ext_info.mosaic_rects) {
                auto mosaic_obj = nlohmann::json::object();
                mosaic_obj["left_top_x"] = item.x / scale_;
                mosaic_obj["left_top_y"] = item.y / scale_;
                mosaic_obj["right_bottom_x"] = (item.x + item.width) / scale_;
                mosaic_obj["right_bottom_y"] = (item.y + item.height) / scale_;
                mosaic_obj["color"] = {255, 255, 255, 1};
                json_obj["mosaics"].emplace_back(std::move(mosaic_obj));
            }

            /*************************** 越界计数可视化处理 **************************/
            int len = back_ext_info.border_points.size();
            for (int i = 0; i < len; i++) {
                auto &points = back_ext_info.border_points[i];
                auto points_array = nlohmann::json::array();
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
                    points_array.emplace_back(point_obj);
                    key_point_array.emplace_back(points_array);
                }

                int index = 0;
                for (auto &[key, values] : back_ext_info.cross_count[i]) {
                    for (const auto &value : values) {
                        auto box_obj = nlohmann::json::object();
                        box_obj["left_top_x"] = (points[0].x + points[1].x) / 2;
                        box_obj["left_top_y"] = (points[0].y + points[1].y) / 2 + index * 30;
                        box_obj["right_bottom_x"] = (points[0].x + points[1].x) / 2;
                        box_obj["right_bottom_y"] = (points[0].y + points[1].y) / 2 + index * 30;
                        box_obj["label"] = key;
                        box_obj["prob"] = value;
                        box_obj["color"] = std::array<int, 4>{255, 0, 0, 1};
                        box_array.emplace_back(box_obj);
                        ++index;
                    }
                }
            }
            /***********************************************************************/

            /******************************** OCR 可视化 ****************************/
            for (const auto &[idx, ocr_info] : back_ext_info.map_ocr_info) {
                auto box_obj = nlohmann::json::object();
                std::vector<cv::Point2f> points;
                for (const auto &point : ocr_info.points) { points.emplace_back(point.x, point.y); }
                auto rect = cv::boundingRect(points);

                box_obj["left_top_x"] = rect.x;
                box_obj["left_top_y"] = rect.y;
                box_obj["right_bottom_x"] = (rect.x + rect.width) / scale_;
                box_obj["right_bottom_y"] = (rect.y + rect.height) / scale_;
                box_obj["prob"] = 0;
                std::string label;
                for (auto &value : ocr_info.labels) { label += value.str; }
                box_obj["label"] = label;
                box_obj["color"] = std::array<int, 4>{255, 0, 0, 1};
                box_array.push_back(box_obj);
            }
            /***********************************************************************/

            /******************************** 分割 可视化 ****************************/
            for (const auto &[idx, contours] : back_ext_info.seg_contours) {
                for (const auto &item : contours) {
                    auto box_obj = nlohmann::json::object();
                    box_obj["left_top_x"] = 0;
                    box_obj["left_top_y"] = 0;
                    box_obj["right_bottom_x"] = 0;
                    box_obj["right_bottom_y"] = 0;
                    box_obj["prob"] = item.area;
                    box_obj["label"] = "面积";
                    box_obj["color"] = std::array<int, 4>{255, 0, 0, 1};
                    box_array.push_back(box_obj);

                    box_obj["left_top_x"] = 0;
                    box_obj["left_top_y"] = 80;
                    box_obj["right_bottom_x"] = 0;
                    box_obj["right_bottom_y"] = 80;
                    box_obj["prob"] = item.length;
                    box_obj["label"] = "长度";
                    box_obj["color"] = std::array<int, 4>{255, 0, 0, 1};
                    box_array.push_back(box_obj);

                    box_obj["left_top_x"] = 0;
                    box_obj["left_top_y"] = 160;
                    box_obj["right_bottom_x"] = 0;
                    box_obj["right_bottom_y"] = 160;
                    box_obj["prob"] = item.volume;
                    box_obj["label"] = "体积";
                    box_obj["color"] = std::array<int, 4>{255, 0, 0, 1};
                    box_array.push_back(box_obj);
                }
            }
            /***********************************************************************/

            /****************************** 目标计数可视化 *****************************/
            int index = 0;
            for (const auto &[key, value] : back_ext_info.target_counts) {
                auto target_obj = nlohmann::json::object();
                target_obj["stat_info"][key] = value;
                target_obj["box"]["left_top_x"] = 0;
                target_obj["box"]["left_top_y"] = 0;
                target_obj["box"]["right_bottom_x"] = index * 80;
                target_obj["box"]["right_bottom_y"] = index * 80;
                target_obj["prob"] = value;
                target_obj["label"] = key;
                target_obj["color"] = std::array<int, 4>{255, 0, 0, 1};
                json_obj["targets"].push_back(target_obj);
                ++index;
            }
            /***********************************************************************/

            auto region_array = nlohmann::json::array();
            for (auto &[key, points] : frame->frame_info->roi_points) {
                auto point_array = nlohmann::json::array();
                for (auto &point : points) { point_array.push_back({point.x / scale_, point.y / scale_}); }
                json_obj["regions_with_label"][key] = point_array;
                region_array.push_back(point_array);
            }

            json_obj["count"] = count;
            json_obj["targets"] = box_array;
            json_obj["key_points"] = key_point_array;
            json_obj["regions"] = region_array;

            std::vector<uchar> buffer;
            auto box_string = json_obj.dump();

            auto tgt_frame =
                image_wrapper::image_resize(frame->frame_info->src_frame->data, frame->frame_info->width() / scale_,
                                            frame->frame_info->height() / scale_);
            if (!jpeg_encoder_) {
                jpeg_encoder_ = std::make_unique<codec::TsingJpegEncode>();
                if (!jpeg_encoder_->init_codecer(frame->frame_info->width(), frame->frame_info->height())) {
                    jpeg_encoder_.reset();
                    return;
                }
            }

            int jpeg_data_size = jpeg_encoder_->codec_image(frame->frame_info->src_frame->data, jpeg_data_.data());
            buffer = std::vector<uchar>(4 + jpeg_data_size + box_string.size());
            memcpy(buffer.data(), &jpeg_data_size, sizeof(jpeg_data_size));
            memcpy(buffer.data() + sizeof(jpeg_data_size), jpeg_data_.data(), jpeg_data_size);
            memcpy(buffer.data() + sizeof(jpeg_data_size) + jpeg_data_size, box_string.data(), box_string.size());

            network::ZmqSocket::get_instance(address_).send(topic, buffer);
        });
    }

private:
    std::string address_{"tcp://*:9000"};
    std::string node_name_;
    int quality_{85};
    int scale_{1};
    bool masking_{false};

    std::array<uint8_t, 4096 * 2160> jpeg_data_;

    std::unique_ptr<codec::TsingJpegEncode> jpeg_encoder_;
};// namespace nodes

}// namespace nodes
}// namespace gddi

#endif// __CVMAT_PUSHER_V2__
