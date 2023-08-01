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

#include "modules/codec/decode_video_v3.h"
#include "modules/codec/demux_stream_v3.h"

#if defined(WITH_BM1684)
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

void running(std::string input_url) {
    auto demuxer = std::make_shared<av_wrapper::Demuxer_v3>();
    auto decoder = std::make_shared<av_wrapper::Decoder_v3>();

    decoder->register_deocde_callback([](const int64_t frame_idx, const std::shared_ptr<AVFrame> &avframe) {
        if (frame_idx % 200 == 0) { printf("=================================== avframe count: %ld\n", frame_idx); }
        return true;
    });

    demuxer->register_open_callback(
        [decoder](const std::shared_ptr<AVCodecParameters> &codecpar) { decoder->open_decoder(codecpar, hw_type); });

    demuxer->register_video_callback([decoder](const int64_t pakcet_idx, const std::shared_ptr<AVPacket> &packet) {
        return decoder->decode_packet(packet);
    });

    demuxer->open_stream(input_url,
                         av_wrapper::DemuxerOptions{.tcp_transport = true, .jump_first_video_i_frame = true});

    while (true) { std::this_thread::sleep_for(std::chrono::seconds(3600)); }
}

int main(int argc, char **argv) {
    std::thread thread_handle[6];
    for (int j = 0; j < 4; j++) { thread_handle[j] = std::thread(running, argv[1]); }
    for (int j = 0; j < 4; j++) {
        if (thread_handle[j].joinable()) { thread_handle[j].join(); }
    }

    return 0;
}