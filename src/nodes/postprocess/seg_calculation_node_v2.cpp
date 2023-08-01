#include "seg_calculation_node_v2.h"
#include "node_msg_def.h"
#include "node_struct_def.h"
#include <exception>
#include <map>
#include <memory>
#include <opencv2/core/hal/interface.h>
#include <opencv2/core/types.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <utility>

namespace gddi {
namespace nodes {

void SegCalculation_v2::on_setup() {
    auto param_matrix = nlohmann::json::parse(param_matrix_);

    try {

        int width = param_matrix["width"].get<int>();
        int height = param_matrix["height"].get<int>();

        if (enable_area_ || enable_length_) {
            // 内参矩阵
            auto vec_camera_matrix = param_matrix["camera_matrix"].get<std::vector<float>>();
            cv::Mat camera_matrix = cv::Mat::zeros(3, 3, CV_32F);
            memcpy(camera_matrix.data, vec_camera_matrix.data(), vec_camera_matrix.size() * sizeof(float));

            // 畸变系数矩阵
            auto vec_dist_coeffs = param_matrix["dist_coeffs"].get<std::vector<float>>();
            cv::Mat dist_coeffs = cv::Mat::zeros(5, 1, CV_32F);
            memcpy(dist_coeffs.data, vec_dist_coeffs.data(), vec_dist_coeffs.size() * sizeof(float));

            // 旋转向量
            auto vec_r_vec = param_matrix["r_vec"].get<std::vector<float>>();
            cv::Mat r_vec = cv::Mat::zeros(3, 1, CV_32F);
            memcpy(r_vec.data, vec_r_vec.data(), vec_r_vec.size() * sizeof(float));

            // 平移向量
            auto vec_t_vec = param_matrix["t_vec"].get<std::vector<float>>();
            cv::Mat t_vec = cv::Mat::zeros(3, 1, CV_32F);
            memcpy(t_vec.data, vec_t_vec.data(), vec_t_vec.size() * sizeof(float));

            // 变换矩阵
            auto vec_perspect_matrix = param_matrix["perspect_matrix"].get<std::vector<float>>();
            cv::Mat p_matrix = cv::Mat::zeros(3, 3, CV_32F);
            memcpy(p_matrix.data, vec_perspect_matrix.data(), vec_perspect_matrix.size() * sizeof(float));

            bev_trans_ = std::make_unique<BevTransform>();
            bev_trans_->SetParam(width, height, camera_matrix, dist_coeffs, r_vec, t_vec, p_matrix);
            seg_by_contours_ = std::make_unique<SegAreaAndMaxLengthByContours>();
        } else if (enable_volume_) {
            // 内参矩阵
            auto vec_camera_matrix = param_matrix["camera_matrix"].get<std::vector<float>>();
            cv::Mat camera_matrix = cv::Mat::zeros(3, 3, CV_32F);
            memcpy(camera_matrix.data, vec_camera_matrix.data(), vec_camera_matrix.size() * sizeof(float));

            // 畸变系数矩阵
            auto vec_dist_coeffs = param_matrix["dist_coeffs"].get<std::vector<float>>();
            cv::Mat dist_coeffs = cv::Mat::zeros(5, 1, CV_32F);
            memcpy(dist_coeffs.data, vec_dist_coeffs.data(), vec_dist_coeffs.size() * sizeof(float));

            camera_distort_ = std::make_unique<CameraDistort>();
            camera_distort_->init_param(camera_matrix, dist_coeffs, cv::Size(width, height));

            seg_by_contours_ = std::make_unique<SegAreaAndMaxLengthByContours>();
            for (auto &item : param_matrix["func_params"]) {
                auto label = item["label"].get<std::string>();
                seg_volumes_.insert(std::make_pair(label, std::make_unique<SegVolume>()));
                seg_volumes_.at(label)->init_param(item["type"].get<FunctionType>(),
                                                   item["values"].get<std::vector<double>>());
            }
        }
    } catch (std::exception &ex) {
        spdlog::error("{}", ex.what());
        quit_runner_(TaskErrorCode::kSegCalculation);
    }
}

void SegCalculation_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    auto clone_frame = std::make_shared<msgs::cv_frame>(frame);
    auto &back_ext_info = clone_frame->frame_info->ext_info.back();
    std::vector<uint8_t> vec_labels;
    for (const auto &[idx, label] : back_ext_info.map_class_label) {
        if (idx != 0) { vec_labels.push_back(idx); }
    }

    if (enable_area_ || enable_length_) {
        cv::Mat output_mask;
        auto input_mask = cv::Mat(clone_frame->frame_info->height(), clone_frame->frame_info->width(), CV_8UC1);
        memcpy(input_mask.data, back_ext_info.seg_map.data(), back_ext_info.seg_map.size());
        bev_trans_->PostProcess(input_mask, &output_mask);
        seg_by_contours_->PostProcess(
            output_mask, vec_labels,
            const_cast<std::map<uint8_t, std::vector<nodes::SegContour>> &>(back_ext_info.seg_contours));
        clone_frame->check_report_callback_ =
            [callback = frame->check_report_callback_](const std::vector<FrameExtInfo> &ext_info) {
                return std::max(FrameType::kReport, callback(ext_info));
            };

        // for (const auto &[idx, item] : back_ext_info.seg_info) {
        //     for (const auto &value : item) {
        //         spdlog::debug("================ idx: {} area: {}, length: {}", idx, value.area, value.length);
        //     }
        // }
    } else if (enable_volume_) {
        cv::Mat output_mask;
        cv::Mat input_mask = cv::Mat(back_ext_info.seg_height, back_ext_info.seg_width, CV_8UC1);
        memcpy(input_mask.data, back_ext_info.seg_map.data(), back_ext_info.seg_map.size());

        camera_distort_->update_mask(input_mask, output_mask);
        seg_by_contours_->PostProcess(output_mask, vec_labels, back_ext_info.seg_contours);

        for (auto seg_iter = back_ext_info.seg_contours.begin(); seg_iter != back_ext_info.seg_contours.end();) {
            auto label = back_ext_info.map_class_label.at(seg_iter->first);
            if (seg_volumes_.count(label) > 0) {
                for (auto iter = seg_iter->second.begin(); iter != seg_iter->second.end();) {
                    if (iter->area > area_thresh_) {
                        seg_volumes_.at(label)->calc_volume(iter->area, iter->volume);
                        if (iter->volume > 0) {
                            ++iter;
                        } else {
                            iter = seg_iter->second.erase(iter);
                        }
                    } else {
                        // 移除小区域误检
                        iter = seg_iter->second.erase(iter);
                    }
                }
                ++seg_iter;
            } else {
                seg_iter = back_ext_info.seg_contours.erase(seg_iter);
            }
        }
    }

    output_result_(clone_frame);
}

}// namespace nodes
}// namespace gddi
