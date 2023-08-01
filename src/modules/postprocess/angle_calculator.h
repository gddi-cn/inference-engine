#pragma once

#include "ellipse_rectifier.h"

// 计算角度，根据给定的roi_points，拟合出椭圆，然后计算将椭圆转换为圆的仿射变换矩阵，使用仿射变换矩阵将点集进行仿射变换，计算出角度
class AngleCalculator {
public:
    void set_roi_points(const std::vector<cv::Point2f> &roi_points);// 传入roi_points
    float get_angle(cv::Point2f point_a, cv::Point2f point_b, cv::Point2f circle_center,
                    bool output_in_clock_wise = true);// 计算角度
    bool is_angle_clock_wise(cv::Point2f point_a, cv::Point2f point_b,
                             cv::Point2f circle_center);// 判断角度是顺时针还是逆时针
    float get_ratio(const cv::Mat &input_points);// 根据输入点计算角度，再根据角度计算比例，不进行仿射变换
    float get_ratio_with_rectify(cv::Mat input_points);// 先对输入点做仿射变换，再计算角度，再根据角度计算比例

private:
    std::unique_ptr<EllipseRectifier> ellipse_rectifier_m;// 椭圆矫正
    cv::Point2f ori_center_m;                             // 椭圆中心
    cv::Point2f ori_start_point_m;                        // 椭圆上的起始点
    cv::Point2f ori_end_point_m;                          // 椭圆上的终止点
    bool is_clock_wise_m;                                 // 角度是否是顺时针
};
