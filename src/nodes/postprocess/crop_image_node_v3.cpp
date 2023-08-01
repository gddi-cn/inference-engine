#include "crop_image_node_v3.h"
#include "node_msg_def.h"
#include <utility>

namespace gddi {
namespace nodes {

#define ALIGN_WIDTH 16
#define ALGIN_HEIGHT 16
#define ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))

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

void CropImage_v3::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    auto clone_frame = std::make_shared<msgs::cv_frame>(frame);

    auto &map_target_box = clone_frame->frame_info->ext_info.back().map_target_box;

    std::vector<std::pair<int, BoxInfo>> vec_target_box(map_target_box.begin(), map_target_box.end());
    std::sort(vec_target_box.begin(), vec_target_box.end(),
              [](const std::pair<int, BoxInfo> &item1, const std::pair<int, BoxInfo> &item2) {
                  return item1.second.box.width * item1.second.box.height
                      > item2.second.box.width * item2.second.box.height
                      || item1.second.prob > item2.second.prob;
              });

    int crop_target_number = std::min(map_target_box.size(), max_target_number_);

    for (int i = 0; i < crop_target_number; i++) {
        clone_frame->frame_info->ext_info.back().crop_rects[vec_target_box[i].first] =
            algin_rect(vec_target_box[i].second.box, clone_frame->frame_info->width(),
                       clone_frame->frame_info->height(), expansion_factor_);
    }

    try {
        clone_frame->frame_info->ext_info.back().crop_images = image_wrapper::image_crop(
            clone_frame->frame_info->src_frame->data, clone_frame->frame_info->ext_info.back().crop_rects);
    } catch (const std::exception &e) { spdlog::error("CropImage_v3: {}", e.what()); }

    output_result_(clone_frame);
}

}// namespace nodes
}// namespace gddi
