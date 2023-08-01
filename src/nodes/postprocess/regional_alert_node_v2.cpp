#include "regional_alert_node_v2.h"
#include "json.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/imgproc.hpp"
#include "utils.hpp"
#include <algorithm>
#include <vector>

namespace gddi {
namespace nodes {

void RegionalAlert_v2::on_setup() {
    auto param_matrix = nlohmann::json::parse(param_matrix_);

    // 内参矩阵
    auto vec_camera_matrix = param_matrix["camera_matrix"].get<std::vector<float>>();
    camera_matrix_ = cv::Mat_<float>(3, 3);
    memcpy(camera_matrix_.data, vec_camera_matrix.data(), vec_camera_matrix.size() * sizeof(float));

    // 畸变系数矩阵
    auto vec_dist_coeffs = param_matrix["dist_coeffs"].get<std::vector<float>>();
    dist_coeffs_ = cv::Mat_<float>(5, 1);
    memcpy(dist_coeffs_.data, vec_dist_coeffs.data(), vec_dist_coeffs.size() * sizeof(float));

    // 旋转向量
    auto vec_r_vec = param_matrix["r_vec"].get<std::vector<float>>();
    r_vec_ = cv::Mat_<float>(3, 1);
    memcpy(r_vec_.data, vec_r_vec.data(), vec_r_vec.size() * sizeof(float));

    // 平移向量
    auto vec_t_vec = param_matrix["t_vec"].get<std::vector<float>>();
    t_vec_ = cv::Mat_<float>(3, 1);
    memcpy(t_vec_.data, vec_t_vec.data(), vec_t_vec.size() * sizeof(float));

    if (!param_matrix["camera_pos"].is_null()) {
        camera_position_ = param_matrix["camera_pos"].get<std::vector<float>>();
    }

    if (!param_matrix["warning_dist"].is_null()) {
        warning_distances_ = param_matrix["warning_dist"].get<std::vector<float>>();
    }
}

void RegionalAlert_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (mask_img_.empty()) {
        mask_img_ = cv::Mat::zeros(frame->frame_info->height(), frame->frame_info->width(), CV_8UC1);
        for (size_t i = 0; i < warning_distances_.size(); i++) {
            fill_circle(50, warning_distances_[i], {camera_position_[0], camera_position_[1], camera_position_[2]},
                        cv::Scalar(i));
        }
    }

    for (auto &[idx, target] : frame->frame_info->ext_info.back().map_target_box) {
        int bottom_cx = target.box.width / 2 + target.box.x;
        int bottom_cy = target.box.height + target.box.y;
        if (bottom_cx > 0 && bottom_cx < mask_img_.size[1] && bottom_cy > 0 && bottom_cy < mask_img_.size[0]) {
            int value = mask_img_.data[bottom_cy * mask_img_.cols + bottom_cx + 0];
            if (track_id_distance_.count(target.track_id) == 0 || value > track_id_distance_.at(target.track_id)) {
                // 区域出现新目标 || 区域目标靠近
                frame->check_report_callback_ =
                    [callback = frame->check_report_callback_](const std::vector<FrameExtInfo> &ext_info) {
                        return std::max(FrameType::kReport, callback(ext_info));
                    };
                track_id_distance_[target.track_id] = value;
                spdlog::debug("=================== track_id: {}, value: {}", target.track_id, value);
            }
            target.distance = warning_distances_[value];
        }
    }

    output_result_(frame);
}

void RegionalAlert_v2::fill_circle(int sample_nums, double radius, cv::Point3d cam_pos, cv::Scalar color) {
    std::vector<double> a = linspace(-M_PI / 2., M_PI / 2, sample_nums);
    std::vector<cv::Point3d> points_3d;
    for (auto it = a.cbegin(); it != a.cend(); it++) {
        points_3d.push_back(cv::Point3d(radius * cos(*it) + cam_pos.x, radius * sin(*it) + cam_pos.y, 0. + cam_pos.z));
    }
    int img_h = mask_img_.size[0];
    int img_w = mask_img_.size[1];
    std::vector<cv::Point2d> points_2d;
    cv::projectPoints(points_3d, r_vec_, t_vec_, camera_matrix_, dist_coeffs_, points_2d);

    std::vector<cv::Point> points_2d_int;
    std::transform(points_2d.begin(), points_2d.end(), std::back_inserter(points_2d_int),
                   [](const cv::Point2d &p) { return (cv::Point)p; });

    // 去掉不在画面内的点
    std::vector<cv::Point> points_2d_filter;
    std::copy_if(points_2d_int.begin(), points_2d_int.end(), std::back_inserter(points_2d_filter),
                 [img_w, img_h](const cv::Point &p) { return p.x > 0 && p.x < img_w && p.y > 0 && p.y < img_h; });

    // 去掉边缘不正常的点
    points_2d_filter = img_point_filter(points_2d_filter);

    // 加上闭合区域
    points_2d_filter.push_back(cv::Point(img_w - 1, points_2d_filter.back().y));
    points_2d_filter.push_back(cv::Point(img_w - 1, img_h - 1));
    points_2d_filter.push_back(cv::Point(0, img_h - 1));
    points_2d_filter.push_back(cv::Point(0, points_2d_filter.front().y));
    cv::fillConvexPoly(mask_img_, points_2d_filter, color);
}

std::vector<double> RegionalAlert_v2::linspace(double start, double end, double num) {
    std::vector<double> linspaced;

    if (num == 0) { return linspaced; }
    if (num == 1) {
        linspaced.push_back(start);
        return linspaced;
    }

    double delta = (end - start) / (num - 1);
    for (int i = 0; i < num - 1; ++i) { linspaced.push_back(start + delta * i); }

    // I want to ensure that start and end are exactly the same as the input
    linspaced.push_back(end);
    return linspaced;
}

std::vector<cv::Point> RegionalAlert_v2::img_point_filter(std::vector<cv::Point2i> points_2d) {
    std::vector<cv::Point> out_points;
    auto last_p = points_2d.cend();
    for (auto it = points_2d.cbegin(); it != points_2d.cend() - 1; it++) {
        if (it->x < (it + 1)->x) {
            out_points.push_back(*it);
            last_p = it + 1;
        }
    }
    if (last_p != points_2d.cend() && last_p != points_2d.cend() - 1) { out_points.push_back(*last_p); }
    return out_points;
}

}// namespace nodes
}// namespace gddi
