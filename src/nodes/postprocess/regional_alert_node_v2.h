//
// Created by zhdotcai on 2022/10/17.
//

#ifndef __REGIONAL_ALERT_NODE_V2_H__
#define __REGIONAL_ALERT_NODE_V2_H__

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"
#include <vector>

namespace gddi {
namespace nodes {
class RegionalAlert_v2 : public node_any_basic<RegionalAlert_v2> {
private:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit RegionalAlert_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("param_matrix", param_matrix_, "参数矩阵");

        // 组件暂不支持数字数组
        // bind_simple_property("camera_position", camera_position_, "相机位置(cm)");
        // bind_simple_property("warning_distances", warning_distances_, "告警距离(cm)");

        register_input_message_handler_(&RegionalAlert_v2::on_cv_image, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }
    ~RegionalAlert_v2() = default;

protected:
    void on_setup() override;
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);
    void fill_circle(int sample_nums, double radius, cv::Point3d cam_pos, cv::Scalar color);
    std::vector<double> linspace(double start, double end, double num);
    std::vector<cv::Point2i> img_point_filter(std::vector<cv::Point2i> points_2d);

private:
    cv::Mat mask_img_;
    std::string param_matrix_;

    cv::Mat camera_matrix_;
    cv::Mat dist_coeffs_;
    cv::Mat r_vec_;
    cv::Mat t_vec_;

    std::vector<float> camera_position_{-200, 0, 0};     //相机位置 unit:cm
    std::vector<float> warning_distances_{500, 300, 200};// 告警距离

    std::map<int, int> track_id_distance_;// 跟踪 ID -- 区域索引
};
}// namespace nodes
}// namespace gddi

#endif// __REGIONAL_ALERT_NODE_V2_H__
