/**
 * @file seg_volume.h
 * @author zhdotcai
 * @brief 
 * @version 0.1
 * @date 2022-12-27
 * 
 * @copyright Copyright (c) 2022 By GDDI
 * 
 */

#include <memory>
#include <opencv2/core.hpp>
#include <vector>

namespace gddi {

enum FunctionType {
    SQRT_LINEAR = 1,
    SQRT_QUADRATIC = 2,
};

class CameraDistort {
public:
    CameraDistort();
    ~CameraDistort();

    void init_param(const cv::Mat &cam_mtx, const cv::Mat &cam_dist, const cv::Size &mask_size);
    void update_mask(const cv::Mat &input_mask, cv::Mat &output_mask);

private:
    cv::Size mask_size_;
    cv::Mat map_x_;
    cv::Mat map_y_;
};

class SegVolume {
public:
    SegVolume();
    ~SegVolume();

    void init_param(const FunctionType type, const std::vector<double> &param);
    bool calc_volume(const double &area, double &volume);

private:
    FunctionType func_type_;
    std::vector<double> func_param_;
};

}// namespace gddi