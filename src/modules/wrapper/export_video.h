#pragma once

#include "modules/codec/encode_video_v3.h"
#include "modules/codec/remux_stream_v3.h"
#include "draw_image.h"
#include "node_struct_def.h"

namespace gddi {

class ExportVideo {
public:
    ExportVideo();
    ~ExportVideo();

    void init_video(const std::string &output_url, const int frame_rate, const int width, const int height);
    void write_frame(const std::shared_ptr<nodes::FrameInfo> &frame_info);
    void close_video();

private:
    std::unique_ptr<av_wrapper::Encoder_v3> encoder_;
    std::unique_ptr<av_wrapper::Remuxer_v3> remuxer_;
};

}// namespace gddi