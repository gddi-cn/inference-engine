#pragma once

#include <opencv2/opencv.hpp>

// 椭圆矫正，根据给定的roi_points，拟合出椭圆，然后将椭圆转换为圆，提供计算仿射变换矩阵的方法，使用仿射变换矩阵将点集进行仿射变换的方法
class EllipseRectifier {
public:
    EllipseRectifier(const std::vector<cv::Point2f>& roi_points);
    cv::Mat draw_ellipse(cv::Mat img); // for debug 绘制椭圆
    cv::Mat ellipse2circle(cv::Mat img, float a, float b, float angle, float e_x, float e_y); // for debug 输入img, 输出仿射变换后的对应的图像
    cv::Mat transform_points(cv::Mat points, cv::Mat transform); // 将点集points进行仿射变换
    cv::Mat get_transform_matrix(float a, float b, float angle, float e_x, float e_y); // 获取仿射变换矩阵

    float x_m, y_m, a_m, b_m, angle_m; // 椭圆的参数，x,y为椭圆中心，a,b为椭圆的长短轴，angle为椭圆的旋转角度
    cv::Mat transform_matrix_m;
};
