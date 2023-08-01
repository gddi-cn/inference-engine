#include "angle_calculator.h"

void AngleCalculator::set_roi_points(const std::vector<cv::Point2f> &roi_points) {
    ellipse_rectifier_m = std::make_unique<EllipseRectifier>(roi_points);
    ori_center_m = cv::Point2f(ellipse_rectifier_m->x_m, ellipse_rectifier_m->y_m);
    ori_start_point_m = roi_points[0];
    ori_end_point_m = roi_points[roi_points.size() - 1];
    is_clock_wise_m = is_angle_clock_wise(roi_points[0], roi_points[1], ori_center_m);
}

float AngleCalculator::get_angle(cv::Point2f point_a, cv::Point2f point_b, cv::Point2f circle_center,
                                 bool output_in_clock_wise) {
    cv::Vec2f vector_a_to_center = point_a - circle_center;
    cv::Vec2f vector_b_to_center = point_b - circle_center;

    float vector_a_to_center_mod = cv::norm(vector_a_to_center);
    float vector_b_to_center_mod = cv::norm(vector_b_to_center);

    float cos_angle = vector_a_to_center.dot(vector_b_to_center) / (vector_a_to_center_mod * vector_b_to_center_mod);
    bool is_clockwise =
        vector_a_to_center[0] * vector_b_to_center[1] - vector_a_to_center[1] * vector_b_to_center[0] > 0;
    float angle = std::acos(cos_angle);

    if (output_in_clock_wise ^ is_clockwise) { angle = 2 * CV_PI - angle; }

    return angle;
}

bool AngleCalculator::is_angle_clock_wise(cv::Point2f point_a, cv::Point2f point_b, cv::Point2f circle_center) {
    cv::Vec2f vector_a = point_a - circle_center;
    cv::Vec2f vector_b = point_b - circle_center;

    float cross_product = vector_a[0] * vector_b[1] - vector_a[1] * vector_b[0];

    if (cross_product == 0) { return vector_a.dot(vector_b) > 0; }

    return cross_product > 0;
}

float AngleCalculator::get_ratio(const cv::Mat &input_points) {
    float angle_start_O_end = get_angle(ori_start_point_m, ori_end_point_m, ori_center_m, is_clock_wise_m);
    float angle_start_O_p1 =
        get_angle(ori_start_point_m, cv::Point2f(input_points.at<float>(0, 0), input_points.at<float>(0, 1)),
                  ori_center_m, is_clock_wise_m);
    float angle_start_O_p2 =
        get_angle(ori_start_point_m, cv::Point2f(input_points.at<float>(1, 0), input_points.at<float>(1, 1)),
                  ori_center_m, is_clock_wise_m);

    return (angle_start_O_p1 + angle_start_O_p2) / 2 / angle_start_O_end;
}

float AngleCalculator::get_ratio_with_rectify(cv::Mat input_points) {
    cv::Mat rectified_points =
        ellipse_rectifier_m->transform_points(input_points, ellipse_rectifier_m->transform_matrix_m);
    cv::Mat points_start_end_center = (cv::Mat_<float>(3, 2) << ori_start_point_m.x, ori_start_point_m.y,
                                       ori_end_point_m.x, ori_end_point_m.y, ori_center_m.x, ori_center_m.y);
    cv::Mat points_start_end_center_rect =
        ellipse_rectifier_m->transform_points(points_start_end_center, ellipse_rectifier_m->transform_matrix_m);
    cv::Point2f points_start_rect(points_start_end_center_rect.at<float>(0, 0),
                                  points_start_end_center_rect.at<float>(0, 1));
    cv::Point2f points_end_rect(points_start_end_center_rect.at<float>(1, 0),
                                points_start_end_center_rect.at<float>(1, 1));
    cv::Point2f points_center_rect(points_start_end_center_rect.at<float>(2, 0),
                                   points_start_end_center_rect.at<float>(2, 1));
    float angle_start_O_end_rect = get_angle(points_start_rect, points_end_rect, points_center_rect, is_clock_wise_m);
    float angle_start_O_p1_rect =
        get_angle(points_start_rect, cv::Point2f(rectified_points.at<float>(0, 0), rectified_points.at<float>(0, 1)),
                  points_center_rect, is_clock_wise_m);
    float angle_start_O_p2_rect =
        get_angle(points_start_rect, cv::Point2f(rectified_points.at<float>(1, 0), rectified_points.at<float>(1, 1)),
                  points_center_rect, is_clock_wise_m);

    return (angle_start_O_p1_rect + angle_start_O_p2_rect) / 2 / angle_start_O_end_rect;
}
