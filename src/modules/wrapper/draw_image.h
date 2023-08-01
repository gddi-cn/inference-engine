#pragma once

#include "modules/cvrelate/graphics.h"
#include "node_struct_def.h"

namespace gddi {

struct Block {
    std::vector<int> coordinate;
    int font_size{28};
};

class DrawImage {
public:
    DrawImage();
    ~DrawImage();

    bool init_drawing(const std::string &font_face, const std::string &background = "");
    void draw_frame(const std::shared_ptr<nodes::FrameInfo> &frame_info);

private:
    std::unique_ptr<graphics::Graphics> graphics_;
    std::map<std::string, Block> blocks_;
    cv::Mat blit_image_;

    std::time_t encode_time_{0};
    int encode_count_{0};
};

}// namespace gddi