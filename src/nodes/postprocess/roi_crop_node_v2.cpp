#include "roi_crop_node_v2.h"
#include "modules/cvrelate/geometry.h"
#include <algorithm>
#include <vector>

#define ALIGN_WIDTH 16
#define ALGIN_HEIGHT 16
#define ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))

namespace gddi {
namespace nodes {

static Rect2f algin_rect(const Rect2f &rect, int img_w, int img_h, float factor) {
    Rect2f resize_rect;

    // 目标框放大
    resize_rect.width = ALIGN(int(rect.width + rect.width * (factor - 1)), ALIGN_WIDTH);
    resize_rect.height = ALIGN(int(rect.height + rect.height * (factor - 1)), ALGIN_HEIGHT);
    if (resize_rect.width > img_w) resize_rect.width = img_w;
    if (resize_rect.height > img_h) resize_rect.height = img_h;

    // 重置顶点座标
    resize_rect.x = rect.x - (resize_rect.width - rect.width) / 2;
    resize_rect.y = rect.y - (resize_rect.height - rect.height) / 2;

    // 边界判断
    if (resize_rect.x < 0) resize_rect.x = 0;
    if (resize_rect.y < 0) resize_rect.y = 0;
    if (resize_rect.x + resize_rect.width > img_w) resize_rect.x = img_w - resize_rect.width;
    if (resize_rect.y + resize_rect.height > img_h) resize_rect.y = img_h - resize_rect.height;

    return resize_rect;
}

void RoiCrop_v2::on_setup() {
    if (!regions_.empty()) {
        char buffer[512] = {0};
        spdlog::debug("############################## ROI "
                      "##############################");
        int region_id_ = 0;
        for (auto &region : regions_) {
            spdlog::debug("[");
            for (auto &point : region) { spdlog::debug("    [{}, {}]", point[0], point[1]); }
            spdlog::debug("]");
            regions_with_label_[std::to_string(region_id_++)] = region;
        }
        for (auto &[key, points] : regions_with_label_) {
            spdlog::debug("[");
            for (auto &point : points) { spdlog::debug("    [{}, {}]", point[0], point[1]); }
            spdlog::debug("]");
        }
        spdlog::debug("#################################################################");
    }
}

void RoiCrop_v2::on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame) {
    // 预处理 ROI 区域
    if (vec_rects_.empty()) {
        if (regions_with_label_.empty()) {
            vec_rects_.emplace_back(Rect2f{0, 0, frame->frame_info->width(), frame->frame_info->height()});
        } else {
            for (auto &[key, points] : regions_with_label_) {
                std::vector<Point2i> new_region_points;
                for (auto &point : points) {
                    Point2i vec_point = {int(point[0] * frame->frame_info->width()),
                                         int(point[1] * frame->frame_info->height())};
                    new_region_points.push_back(vec_point);
                }

                std::map<std::string, std::vector<Point2i>> tmp_roi_points;
                for (auto iter = roi_points_.begin(); iter != roi_points_.end();) {
                    if (geometry::has_intersection(iter->second, new_region_points)) {
                        tmp_roi_points.insert(*iter);
                        roi_points_.erase(iter);
                    } else {
                        iter++;
                    }
                }

                if (!tmp_roi_points.empty()) {
                    roi_points_[key] = geometry::merge_region(new_region_points, tmp_roi_points);
                } else {
                    roi_points_[key] = new_region_points;
                }

                auto x_compare = [](const std::vector<float> &first, const std::vector<float> &second) {
                    return first[0] < second[0];
                };

                auto y_compare = [](const std::vector<float> &first, const std::vector<float> &second) {
                    return first[1] < second[1];
                };

                auto min_x =
                    (*std::min_element(points.begin(), points.end(), x_compare))[0] * frame->frame_info->width();
                auto min_y =
                    (*std::min_element(points.begin(), points.end(), y_compare))[1] * frame->frame_info->height();
                auto max_x =
                    (*std::max_element(points.begin(), points.end(), x_compare))[0] * frame->frame_info->width();
                auto max_y =
                    (*std::max_element(points.begin(), points.end(), y_compare))[1] * frame->frame_info->height();
                vec_rects_.emplace_back(Rect2f{min_x, min_y, max_x - min_x, max_y - min_y});
            }
        }
    }

    auto clone_frame = std::make_shared<msgs::cv_frame>(frame);
    clone_frame->frame_info->roi_points = roi_points_;

    nodes::FrameExtInfo ext_info(AlgoType::kDetection, "", "", 0);
    ext_info.flag_crop = true;

    int index = 0;
    for (auto &item : vec_rects_) {
        ext_info.map_target_box.insert(std::make_pair(index,
                                                      BoxInfo{.prev_id = index,
                                                              .class_id = 0,
                                                              .prob = 1,
                                                              .box = item,
                                                              .roi_id = std::to_string(index),
                                                              .track_id = index}));
        ext_info.crop_rects.insert(std::make_pair(
            index, algin_rect(item, clone_frame->frame_info->width(), clone_frame->frame_info->height(), 1)));
        index++;
    }

    try {
        ext_info.crop_images = image_wrapper::image_crop(clone_frame->frame_info->src_frame->data, ext_info.crop_rects);
    } catch (const std::exception &e) { spdlog::error("CropImage_v2: {}", e.what()); }

    clone_frame->frame_info->ext_info.emplace_back(ext_info);

    output_image_(clone_frame);
}

}// namespace nodes
}// namespace gddi