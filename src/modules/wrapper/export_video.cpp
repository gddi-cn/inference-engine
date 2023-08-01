#include "export_video.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#if defined(WITH_NVIDIA)
#define DEVICE_TYPE AV_HWDEVICE_TYPE_CUDA
#elif defined(WITH_MLU220) || defined(WITH_MLU270)
#define DEVICE_TYPE AV_HWDEVICE_TYPE_MLU
#else
#define DEVICE_TYPE AV_HWDEVICE_TYPE_NONE
#endif

namespace gddi {

ExportVideo::ExportVideo() {
    encoder_ = std::make_unique<av_wrapper::Encoder_v3>();
    remuxer_ = std::make_unique<av_wrapper::Remuxer_v3>();
}

ExportVideo::~ExportVideo() { close_video(); }

void ExportVideo::init_video(const std::string &output_url, const int frame_rate, const int width, const int height) {
    encoder_ = std::make_unique<av_wrapper::Encoder_v3>();
    remuxer_ = std::make_unique<av_wrapper::Remuxer_v3>();
    encoder_->register_open_callback([this, output_url](const std::shared_ptr<AVCodecParameters> &codecpar,
                                                        const AVRational &timebase, const AVRational &framerate) {
        if (!remuxer_->open_stream(output_url, codecpar, timebase, framerate)) {
            throw std::runtime_error("Failed to open remuxer");
        }
    });

    encoder_->register_packet_callback([this](const std::shared_ptr<AVPacket> &packet) {
        if (!remuxer_->write_packet(packet)) { throw std::runtime_error("Failed to write packet"); }
    });

    if (!encoder_->open_encoder(av_wrapper::EncodeOptions{.width = width, .height = height, .framerate = frame_rate},
                                DEVICE_TYPE)) {
        throw std::runtime_error("Failed to open encoder");
    }
}

void ExportVideo::write_frame(const std::shared_ptr<nodes::FrameInfo> &frame_info) {
    auto &back_ext_info = frame_info->ext_info.back();

#ifdef WITH_BM1684
    bm_image bgr_image;
    cv::bmcv::toBMI(frame_info->tgt_frame, &bgr_image);
    auto yuv_image = image_wrapper::image_cvt_format(bgr_image);
    int plane_num = bm_image_get_plane_num(*yuv_image);
    bm_device_mem_t yuv_mem[plane_num];
    bm_image_get_device_mem(*yuv_image, yuv_mem);

    auto yuv_data = std::vector<uint8_t>(frame_info->tgt_frame.cols * frame_info->tgt_frame.rows * 3 / 2);
    auto ptr = yuv_data.data();
    for (int i = 0; i < plane_num; i++) {
        bm_memcpy_d2s(*image_wrapper::bm_handle, ptr, yuv_mem[i]);
        ptr += yuv_mem[i].size;
    }
    encoder_->encode_frame(yuv_data.data(), frame_info->tgt_frame.cols, frame_info->tgt_frame.rows);
#else
    auto avframe = std::shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *frame) { av_frame_free(&frame); });
    auto buffer =
        std::shared_ptr<uint8_t>((uint8_t *)av_malloc(av_image_get_buffer_size(
                                     AV_PIX_FMT_NV12, frame_info->tgt_frame.cols, frame_info->tgt_frame.rows, 1)),
                                 [](uint8_t *buf) { av_free(buf); });
    avframe->width = frame_info->tgt_frame.cols;
    avframe->height = frame_info->tgt_frame.rows;
    avframe->format = AV_PIX_FMT_NV12;
    int cvLinesizes[1];
    cvLinesizes[0] = frame_info->tgt_frame.step1();

    av_image_fill_arrays(avframe->data, avframe->linesize, buffer.get(), AV_PIX_FMT_NV12, avframe->width,
                         avframe->height, 1);
    SwsContext *conversion;
    if (frame_info->tgt_frame.channels() == 3) {
        conversion =
            sws_getContext(frame_info->tgt_frame.cols, frame_info->tgt_frame.rows, AV_PIX_FMT_BGR24, avframe->width,
                           avframe->height, AV_PIX_FMT_NV12, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    } else {
        conversion =
            sws_getContext(frame_info->tgt_frame.cols, frame_info->tgt_frame.rows, AV_PIX_FMT_BGRA, avframe->width,
                           avframe->height, AV_PIX_FMT_NV12, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    }
    sws_scale(conversion, &frame_info->tgt_frame.data, cvLinesizes, 0, frame_info->tgt_frame.rows, avframe->data,
              avframe->linesize);
    sws_freeContext(conversion);
    encoder_->encode_frame(avframe);
#endif
}

void ExportVideo::close_video() {
    encoder_->close_encoder();
    remuxer_->close_stream();
}

}// namespace gddi