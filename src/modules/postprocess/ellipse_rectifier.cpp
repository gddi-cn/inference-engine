#include "ellipse_rectifier.h"

EllipseRectifier::EllipseRectifier(const std::vector<cv::Point2f>& roi_points) {
    // roi_points shape: (n, 2), n>=5
    cv::RotatedRect ellipse = cv::fitEllipse(roi_points); // 拟合椭圆
    x_m = ellipse.center.x;
    y_m = ellipse.center.y;
    a_m = ellipse.size.width / 2.0;
    b_m = ellipse.size.height / 2.0;
    angle_m = ellipse.angle;
    transform_matrix_m = get_transform_matrix(a_m, b_m, angle_m, x_m, y_m);
}

cv::Mat EllipseRectifier::draw_ellipse(cv::Mat img) {
    cv::ellipse(img, cv::Point2f(x_m, y_m), cv::Size2f(a_m, b_m), angle_m, 0, 360, cv::Scalar(0, 0, 255), 2);
    return img;
}


cv::Mat EllipseRectifier::ellipse2circle(cv::Mat img, float a, float b, float angle, float e_x, float e_y) {
    int h = img.rows;
    int w = img.cols;
    float scale = a / b;

    // Rotate to make ellipse horizontal
    cv::Mat rotation_matrix = cv::getRotationMatrix2D(cv::Point2f(e_x, e_y), angle, 1);
    rotation_matrix.at<double>(0, 0) *= scale;
    rotation_matrix.at<double>(1, 2) *= scale;

    // Calculate the new ellipse center after rotation
    cv::Mat center_pt = (cv::Mat_<double>(3, 1) << e_x, e_y, 1);
    cv::Mat rt_matrix_homo = cv::Mat::zeros(3, 3, CV_64F);
    rotation_matrix.copyTo(rt_matrix_homo(cv::Rect(0, 0, rotation_matrix.cols, rotation_matrix.rows)));
    rt_matrix_homo.at<double>(2, 2) = 1;
    center_pt = rt_matrix_homo(cv::Rect(0, 0, 2, 2)) * center_pt + rt_matrix_homo(cv::Rect(2, 0, 1, 2));

    // Rotate back to the original angle
    cv::Mat rt_matrix2_homo = cv::Mat::zeros(3, 3, CV_64F);
    cv::Point2f center_pt_2f = cv::Point2f(center_pt.at<double>(0, 0), center_pt.at<double>(1, 0));
    cv::Mat affine_matrix = cv::getRotationMatrix2D(center_pt_2f, -angle, 1);
    affine_matrix.copyTo(rt_matrix2_homo(cv::Rect(0, 0, affine_matrix.cols, affine_matrix.rows)));
    rt_matrix2_homo.at<double>(2, 2) = 1;
    affine_matrix = (rt_matrix2_homo * rt_matrix_homo)(cv::Rect(0, 0, rotation_matrix.cols, rotation_matrix.rows));

    cv::Mat circle_out;
    cv::warpAffine(img, circle_out, affine_matrix, img.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
    return circle_out;
}

cv::Mat EllipseRectifier::transform_points(cv::Mat points, cv::Mat transform) {
    cv::Mat points_trans = points.clone();
    points_trans.push_back(cv::Mat::ones(1, points.cols, points.type()));
    points_trans = transform * points_trans.t();
    points_trans = points_trans.t();
    return points_trans(cv::Rect(0, 0, points.cols, points.rows));
}

cv::Mat EllipseRectifier::get_transform_matrix(float a, float b, float angle, float e_x, float e_y) {
    // Rotate to make ellipse horizontal
    cv::Mat rotation_matrix = cv::getRotationMatrix2D(cv::Point2f(e_x, e_y), angle, 1);
    rotation_matrix.at<double>(0, 0) *= a / b;
    rotation_matrix.at<double>(1, 1) *= a / b;
    rotation_matrix.at<double>(1, 2) *= a / b;
    cv::Mat transform = rotation_matrix(cv::Rect(0, 0, 2, 2));

    // Calculate the new ellipse center after rotation
    cv::Mat center_pt = (cv::Mat_<double>(2, 1) << e_x, e_y);
    cv::Mat rt_matrix_homo = cv::Mat::zeros(3, 3, CV_64F);
    rotation_matrix.copyTo(rt_matrix_homo(cv::Rect(0, 0, rotation_matrix.cols, rotation_matrix.rows)));
    rt_matrix_homo.at<double>(2, 2) = 1;
    center_pt = rt_matrix_homo(cv::Rect(0, 0, 2, 2)) * center_pt + rt_matrix_homo(cv::Rect(2, 0, 1, 2));

    // Rotate back to the original angle
    cv::Mat rt_matrix2_homo = cv::Mat::zeros(3, 3, CV_64F);
    cv::Point2f center_pt_2f = cv::Point2f(center_pt.at<double>(0, 0), center_pt.at<double>(1, 0));
    cv::Mat affine_matrix = cv::getRotationMatrix2D(center_pt_2f, -angle, 1);
    affine_matrix.copyTo(rt_matrix2_homo(cv::Rect(0, 0, affine_matrix.cols, affine_matrix.rows)));
    rt_matrix2_homo.at<double>(2, 2) = 1;
    affine_matrix = (rt_matrix2_homo * rt_matrix_homo)(cv::Rect(0, 0, rotation_matrix.cols, rotation_matrix.rows));

    return affine_matrix;
}

