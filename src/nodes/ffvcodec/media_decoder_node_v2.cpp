#include "media_decoder_node_v2.h"
#include "codec/mpp_decoder.h"
#include "node_msg_def.h"
#include "node_struct_def.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace gddi {
namespace nodes {

void MediaDecoder_v2::on_setup() {
    hw_type_ = AV_HWDEVICE_TYPE_NONE;
    demuxer_ = std::make_shared<av_wrapper::Demuxer_v3>();
    decoder_ = std::make_shared<av_wrapper::Decoder_v3>();
    open_demuxer();
}

void MediaDecoder_v2::open_demuxer() {
    demuxer_->register_open_callback([this](const std::shared_ptr<AVCodecParameters> &codecpar) {
        frame_rate_ = demuxer_->get_video_frame_rate();

        std::string lower_url;
        for (char c : input_url_) { lower_url += std::tolower(c); }
        if (strstr(lower_url.c_str(), "rtsp://") != nullptr || strstr(lower_url.c_str(), "rtmp://") != nullptr) {
            this->task_type_ = TaskType::kCamera;
        } else if (strstr(lower_url.c_str(), ".mp4") != nullptr) {
            this->task_type_ = TaskType::kVideo;
        } else if (strstr(lower_url.c_str(), ".jpg") != nullptr || strstr(lower_url.c_str(), ".png") != nullptr) {
            this->task_type_ = TaskType::kImage;
            frame_rate_ = 1;
        } else {
            this->task_type_ = TaskType::kUndefined;
        }

        if (frame_rate_ > 30) { spdlog::warn("Detect frame rate: {}", frame_rate_); }
        open_decoder(codecpar);
    });

    demuxer_->register_video_callback([this](const int64_t pakcet_idx, const std::shared_ptr<AVPacket> &packet) {
        return decoder_->decode_packet(packet);
    });

    demuxer_->register_close_callback([this](bool normal_exit) {
        if (task_type_ == TaskType::kCamera) {
            quit_runner_(normal_exit ? TaskErrorCode::kNormal : TaskErrorCode::kDemuxer);
        } else if (task_type_ == TaskType::kVideo) {
            auto frame = std::make_shared<msgs::cv_frame>(task_name_, task_type_, frame_rate_, gddi::FrameType::kNone);
            output_cv_frame_(frame);
        }
    });

    demuxer_->open_stream(input_url_,
                          av_wrapper::DemuxerOptions{.tcp_transport = true, .jump_first_video_i_frame = true});
}

void MediaDecoder_v2::open_decoder(const std::shared_ptr<AVCodecParameters> &codecpar) {
    decoder_->register_open_callback([this](const std::shared_ptr<AVCodecParameters> &codecpar) {
        output_open_decoder_(std::make_shared<msgs::av_decode_open>(codecpar.get()));
    });

    decoder_->register_deocde_callback([this](const int64_t frame_idx, const std::shared_ptr<AVFrame> &avframe) {
        if (frame_rate_ > 60 || frame_rate_ <= 0) {
            if (frame_idx == 1) { timestamp_ = std::chrono::system_clock::now(); }
            if (frame_idx <= 25 * 60) {
                // 计算 1 min 的平均帧率
                if (this->task_type_ == TaskType::kCamera) {
                    frame_rate_ = frame_idx * 1000.0
                        / std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()
                                                                                - timestamp_)
                              .count();
                } else if (this->task_type_ == TaskType::kImage) {
                    frame_rate_ = 1;
                }
            }
        }
        if (frame_idx == 25 * 60) { spdlog::warn("Final frame rate: {}", frame_rate_); }

        auto frame = std::make_shared<msgs::cv_frame>(task_name_, task_type_, frame_rate_);
        auto mem_obj = image_wrapper::image_from_avframe(mem_pool_, avframe);
        frame->frame_info = std::make_shared<nodes::FrameInfo>(frame_idx, mem_obj);
        output_cv_frame_(frame);
        return true;
    });

    if (decoder_->open_decoder(codecpar, hw_type_)) {
        spdlog::info("Success to open decoder");
    } else {
        quit_runner_(TaskErrorCode::kDecoder);
    }
}

}// namespace nodes
}// namespace gddi