/**
 * @file test_decode_v3.cpp
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 0.1
 * @date 2022-11-21
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "codec/decode_video_v3.h"
#include "codec/demux_stream_v3.h"
#include "codec/encode_video_v3.h"
#include "codec/remux_stream_v3.h"
#include <bits/types/FILE.h>
#include <cstdint>
#include <cstdio>
#include <memory>

#if defined(WITH_BM1684)
#include "wrapper/bm1684_wrapper.hpp"
AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_NONE;
#elif defined(WITH_MLU220) || defined(WITH_MLU270)
AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_MLU;
#elif defined(WITH_NVIDIA)
AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_CUDA;
#elif defined(WITH_INTEL)
AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_VAAPI;
#else
AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_NONE;
#endif

void running(const std::string &input_url) {
    bool running = true;
    auto demuxer = std::make_shared<av_wrapper::Demuxer_v3>();
    auto decoder = std::make_shared<av_wrapper::Decoder_v3>();
    auto encoder = std::make_shared<av_wrapper::Encoder_v3>();
    auto remuxer = std::make_shared<av_wrapper::Remuxer_v3>();

    encoder->register_open_callback([&remuxer](const std::shared_ptr<AVCodecParameters> &codecpar,
                                               const AVRational &timebase, const AVRational &framerate) {
        remuxer->open_stream("remuxer-video.mp4", codecpar, timebase, framerate);
    });

    encoder->register_packet_callback([&remuxer](const std::shared_ptr<AVPacket> &packet) {
        remuxer->write_packet(packet);
    });

    decoder->register_open_callback([&encoder](const std::shared_ptr<AVCodecParameters> &codecpar) {
        encoder->open_encoder(
            av_wrapper::EncodeOptions{.width = codecpar->width, .height = codecpar->height, .framerate = 25}, hw_type);
    });

    decoder->register_deocde_callback([&encoder](const int64_t frame_idx, const std::shared_ptr<AVFrame> &avframe) {
#if defined (WITH_BM1684)
        auto src_frame = std::shared_ptr<bm_image>(new bm_image, [](bm_image *ptr) {
            bm_image_destroy(*ptr);
            delete ptr;
        });

        int stride[]{avframe->width, avframe->width};
        bm_image_create(*gddi::image_wrapper::bm_handle, avframe->height, avframe->width, FORMAT_NV12,
                        DATA_TYPE_EXT_1N_BYTE, src_frame.get(), stride);
        bm_image_alloc_dev_mem_heap_mask(*src_frame, 6);
        bm_image_from_frame(*gddi::image_wrapper::bm_handle, *avframe, *src_frame);

        auto yuv420p = gddi::image_wrapper::image_cvt_format(src_frame);

        auto enc_frame = std::shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *frame) { av_frame_free(&frame); });
        if (bm_image_to_avframe(*gddi::image_wrapper::bm_handle, yuv420p.release(), enc_frame.get()) != BM_SUCCESS) {
            return false;
        }

        encoder->encode_frame(enc_frame);
#endif
        return true;
    });

    demuxer->register_open_callback(
        [decoder](const std::shared_ptr<AVCodecParameters> &codecpar) { decoder->open_decoder(codecpar, hw_type); });

    demuxer->register_video_callback([decoder](const int64_t pakcet_idx, const std::shared_ptr<AVPacket> &packet) {
        return decoder->decode_packet(packet);
    });

    demuxer->register_close_callback([&remuxer, &running](const bool) {
        remuxer->close_stream();
        running = false;
    });

    demuxer->open_stream(input_url,
                         av_wrapper::DemuxerOptions{.tcp_transport = true, .jump_first_video_i_frame = true});

    while (running) { std::this_thread::sleep_for(std::chrono::seconds(1)); }
}

int main(int argc, char **argv) {
    std::thread thread_handle[6];
    for (int j = 0; j < 1; j++) { thread_handle[j] = std::thread(running, argv[1]); }
    for (int j = 0; j < 1; j++) {
        if (thread_handle[j].joinable()) { thread_handle[j].join(); }
    }

    return 0;
}