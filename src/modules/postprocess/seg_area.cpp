
#include "seg_area.h"
#include <cstddef>
#include <cstdint>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace gddi {

SegTotalArea::SegTotalArea(const uint8_t target_label) { target_label_ = target_label; }

size_t SegTotalArea::PostProcess(const std::vector<uint8_t> &input_mask) {
    size_t area = 0;

    for (int i = 0; i < input_mask.size(); i++) {
        if (input_mask[i] == target_label_) { area++; }
    }

    return area;
}

double get_max_length_in_contours(const std::vector<cv::Point> &contour) {
    int max_length = -1;
    for (auto it = contour.cbegin(); it != contour.cend(); it++) {
        for (auto it2 = it + 1; it2 != contour.cend(); it2++) {
            max_length =
                std::max(max_length, (it->x - it2->x) * (it->x - it2->x) + (it->y - it2->y) * (it->y - it2->y));
        }
    }
    return std::sqrt(max_length);
}

void cal_map_matrix(const cv::Mat &perspect_mat, const cv::Size &img_size, cv::Mat *out_map_x, cv::Mat *out_map_y) {
    cv::Mat srcTM, transformation_x, transformation_y;
    srcTM = perspect_mat.clone();// If WARP_INVERSE, set srcTM to transformationMatrix
    srcTM = srcTM.inv();
    // cv::Size out_size(img_w, img_h);
    out_map_x->create(img_size, CV_32FC1);
    out_map_y->create(img_size, CV_32FC1);

    double M11, M12, M13, M21, M22, M23, M31, M32, M33;
    M11 = (double)srcTM.at<float>(0, 0);
    M12 = (double)srcTM.at<float>(0, 1);
    M13 = (double)srcTM.at<float>(0, 2);
    M21 = (double)srcTM.at<float>(1, 0);
    M22 = (double)srcTM.at<float>(1, 1);
    M23 = (double)srcTM.at<float>(1, 2);
    M31 = (double)srcTM.at<float>(2, 0);
    M32 = (double)srcTM.at<float>(2, 1);
    M33 = (double)srcTM.at<float>(2, 2);

    float *out_map_x_data = (float *)out_map_x->data;
    float *out_map_y_data = (float *)out_map_y->data;

    for (int y = 0; y < img_size.height; y++) {
        double fy = (double)y;
        for (int x = 0; x < img_size.width; x++) {
            double fx = (double)x;
            double w = ((M31 * fx) + (M32 * fy) + M33);
            w = w != 0.0f ? 1.f / w : 0.0f;
            *(out_map_x_data + y * img_size.width + x) = (float)((M11 * fx) + (M12 * fy) + M13) * w;
            *(out_map_y_data + y * img_size.width + x) = (float)((M21 * fx) + (M22 * fy) + M23) * w;
        }
    }
}

void SegAreaAndMaxLengthByContours::PostProcess(const cv::Mat &input_mask, const std::vector<uint8_t> target_label,
                                                std::map<uint8_t, std::vector<nodes::SegContour>> &output) {
    for (const auto &value : target_label) {
        cv::Mat binary_mask = cv::Mat::zeros(input_mask.rows, input_mask.cols, CV_8UC1);
        int rows = input_mask.rows;
        int cols = input_mask.cols * input_mask.channels();
        int area = 0;
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                cv::uint8_t val = input_mask.at<cv::uint8_t>(i, j);
                if (val == value) { binary_mask.at<cv::uint8_t>(i, j) = 255; }
            }
        }

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        for (auto it = contours.cbegin(); it < contours.cend(); it++) {
            auto rect = cv::minAreaRect(*it).center;
            output[value].emplace_back(
                nodes::SegContour{rect.x, rect.y, cv::contourArea(*it), get_max_length_in_contours(*it)});
        }
    }
}

BevTransform::BevTransform() {
    cam_mtx_ = cv::Mat::zeros(3, 3, CV_32F);
    cam_dist_ = cv::Mat::zeros(5, 1, CV_32F);
    perspect_mat_ = cv::Mat::zeros(3, 3, CV_32F);
}

void BevTransform::SetParam(const int width, const int height, const cv::Mat &cam_mtx, const cv::Mat &cam_dist,
                            const cv::Mat &r_vec, const cv::Mat &t_vec, const cv::Mat &perspect_mat) {
    cam_mtx.copyTo(cam_mtx_);
    cam_dist.copyTo(cam_dist_);
    perspect_mat.copyTo(perspect_mat_);

    cv::initUndistortRectifyMap(cam_mtx_, cam_dist_, cv::Mat(), cam_mtx_, cv::Size(width, height), CV_32FC1, map1_x_,
                                map1_y_);
    cal_map_matrix(perspect_mat, cv::Size(width, height), &map2_x_, &map2_y_);
}

void BevTransform::PostProcess(const cv::Mat &input_mask, cv::Mat *output_mask) {
    int img_h = input_mask.size[0];
    int img_w = input_mask.size[1];

    cv::Mat undistort_img;
    cv::remap(input_mask, undistort_img, map1_x_, map1_y_, cv::INTER_NEAREST);
    cv::remap(undistort_img, *output_mask, map2_x_, map2_y_, cv::INTER_NEAREST);

    // cv::undistort(input_mask, undistort_img, cam_mtx_, cam_dist_, cam_mtx_);
    // cv::warpPerspective(undistort_img, *output_mask, perspect_mat_, cv::Size(img_w * 2, img_h * 2),
    //                     cv::INTER_NEAREST);
}

}// namespace gddi