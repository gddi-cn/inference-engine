#ifndef __SEG_AREA_H__
#define __SEG_AREA_H__

#include "node_struct_def.h"
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <vector>

namespace gddi {

class SegTotalArea {
public:
    SegTotalArea(const uint8_t target_label);
    size_t PostProcess(const std::vector<uint8_t> &input_mask);

private:
    uint8_t target_label_;
};

class SegAreaAndMaxLengthByContours {
public:
    void PostProcess(const cv::Mat &input_mask, const std::vector<uint8_t> target_label,
                     std::map<uint8_t, std::vector<nodes::SegContour>> &output);
};

class BevTransform {
public:
    BevTransform();
    void SetParam(const int width, const int height, const cv::Mat &cam_mtx, const cv::Mat &cam_dist,
                  const cv::Mat &r_vec, const cv::Mat &t_vec, const cv::Mat &perspect_mat);
    void PostProcess(const cv::Mat &input_mask, cv::Mat *output_mask);

private:
    cv::Mat cam_mtx_;
    cv::Mat cam_dist_;
    cv::Mat perspect_mat_;

    cv::Mat map1_x_;
    cv::Mat map1_y_;
    cv::Mat map2_x_;
    cv::Mat map2_y_;
};

}// namespace gddi

#endif//__SEG_AREA_H__
