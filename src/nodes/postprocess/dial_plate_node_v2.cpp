#include "dial_plate_node_v2.h"
#include "modules/postprocess/angle_calculator.h"

namespace gddi {
namespace nodes {

void DialPlate_v2::on_setup() { calculator_ = std::make_unique<AngleCalculator>(); }

void DialPlate_v2::on_cv_frame(const std::shared_ptr<msgs::cv_frame> &frame) {
    cv::Mat image = image_wrapper::image_to_mat(frame->frame_info->src_frame->data);

    auto &front_ext_info = frame->frame_info->ext_info.front();
    auto &back_ext_info = frame->frame_info->ext_info.back();
    for (const auto &[idx, points] : back_ext_info.map_key_points) {
        std::vector<cv::Point2f> roi_points;
        for (const auto &point : frame->frame_info->roi_points[back_ext_info.map_target_box[idx].roi_id]) {
            roi_points.emplace_back(point.x, point.y);
        }

        cv::Mat input_points(std::vector<cv::Point2f>({{points[0].x, points[0].y}, {points[1].x, points[1].y}}), true);

        calculator_->set_roi_points(roi_points);
        back_ext_info.map_target_box[idx].prob = calculator_->get_ratio(input_points);

        // cv::Mat crop_image;
        // front_ext_info.crop_images[idx].download(crop_image);
        // cv::line(crop_image,
        //          {points[0].x - front_ext_info.crop_rects[idx].x, points[0].y - front_ext_info.crop_rects[idx].y},
        //          {points[1].x - front_ext_info.crop_rects[idx].x, points[1].y - front_ext_info.crop_rects[idx].y},
        //          cv::Scalar(0, 255, 0), 2);

        // for (const auto &point : roi_points) {
        //     cv::circle(crop_image,
        //                {point.x - front_ext_info.crop_rects[idx].x, point.y - front_ext_info.crop_rects[idx].y}, 2,
        //                cv::Scalar(0, 0, 255), -1);
        // }

        // cv::imwrite("crop_image" + std::to_string(idx) + ".jpg", crop_image);
    }
    output_result_(frame);
}
}// namespace nodes
}// namespace gddi