#include "seg_volume.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

namespace gddi {

CameraDistort::CameraDistort() {}

CameraDistort::~CameraDistort() {}

void CameraDistort::init_param(const cv::Mat &cam_mtx, const cv::Mat &cam_dist, const cv::Size &mask_size) {
    mask_size_ = mask_size;
    cv::initUndistortRectifyMap(cam_mtx, cam_dist, cv::Mat(), cam_mtx, mask_size, CV_32FC1, map_x_, map_y_);
}
void CameraDistort::update_mask(const cv::Mat &input_mask, cv::Mat &output_mask) {
    auto tmp_input_mask = input_mask;
    if (tmp_input_mask.cols != mask_size_.width || tmp_input_mask.rows != mask_size_.height) {
        cv::resize(input_mask, tmp_input_mask, mask_size_);
    }
    cv::remap(tmp_input_mask, output_mask, map_x_, map_y_, cv::INTER_NEAREST);
}

SegVolume::SegVolume() {}
SegVolume::~SegVolume() {}

void SegVolume::init_param(const FunctionType type, const std::vector<double> &param) {
    func_type_ = type;
    func_param_ = param;
}

bool SegVolume::calc_volume(const double &area, double &volume) {
    if (func_type_ == SQRT_LINEAR) {
        double sqrt_value = std::sqrt(area);
        volume = func_param_[0] * sqrt_value + func_param_[1];
    } else if (func_type_ == SQRT_QUADRATIC) {
        double sqrt_value = std::sqrt(area);
        volume = func_param_[0] * sqrt_value * sqrt_value + func_param_[1] * sqrt_value + func_param_[2];
        volume = std::min(volume, func_param_[4]);
        volume = std::max(volume, func_param_[3]);
    }

    return true;
}

}// namespace gddi