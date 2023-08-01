#include "graphics_node_v2.h"

namespace gddi {
namespace nodes {

const int skeleton[][2] = {{15, 13}, {13, 11}, {16, 14}, {14, 12}, {11, 12}, {5, 11}, {6, 12}, {5, 6}, {5, 7}, {6, 8},
                           {7, 9},   {8, 10},  {1, 2},   {0, 1},   {0, 2},   {1, 3},  {2, 4},  {3, 5}, {4, 6}};

void Graphics_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == FrameType::kNone) {
        output_result_(frame);
        return;
    }

    graphics_->set_image(image_wrapper::image_to_mat(frame->frame_info->src_frame->data));

    for (auto &[key, points] : frame->frame_info->roi_points) { graphics_->draw_polygon_fill(points, {0, 0, 116, 255 * 0.2}); }

    auto &back_ext_info = frame->frame_info->ext_info.back();
    for (const auto &[idx, item] : back_ext_info.map_target_box) {
        graphics_->draw_rect(item.box, item.prob, back_ext_info.map_class_color.at(item.class_id), 4);
        graphics_->draw_text_fill(Point2i{(int)item.box.x - 4, (int)item.box.y},
                                  back_ext_info.map_class_label.at(item.class_id) + " "
                                      + std::to_string(item.prob).substr(0, 4),
                                  back_ext_info.map_class_color.at(item.class_id));
    }

    /********************************** POSE ************************************/
    for (const auto &[idx, points] : back_ext_info.map_key_points) {
        for (const auto &bone : skeleton) {
            Scalar color;
            if (bone[0] < 5 || bone[1] < 5) color = {0, 255, 0, 255};
            else if (bone[0] > 12 || bone[1] > 12)
                color = {255, 0, 0};
            else if (bone[0] > 4 && bone[0] < 11 && bone[1] > 4 && bone[1] < 11)
                color = {0, 255, 255, 255};
            else
                color = {255, 0, 255, 255};
            graphics_->draw_line(Point2i{(int)points[bone[0]].x, (int)points[bone[0]].y},
                                 Point2i{(int)points[bone[1]].x, (int)points[bone[1]].y}, color, 2);
        }
        for (auto &item : points) {
            Scalar color;
            if (item.number < 5) color = {0, 255, 0, 255};
            else if (item.number > 10)
                color = {255, 0, 0, 255};
            else
                color = {0, 255, 255, 255};
            graphics_->draw_circle(Point2i{(int)item.x, (int)item.y}, color, 5);
        }
    }

    frame->frame_info->tgt_frame = graphics_->get_image();

    output_result_(frame);
}

}// namespace nodes
}// namespace gddi