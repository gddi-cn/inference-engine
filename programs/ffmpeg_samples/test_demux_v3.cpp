//
// Created by zhdotcai on 4/25/22.
//

#include "modules/codec/demux_stream_v3.h"

void running(std::string input_url) {
    auto demuxer = std::make_unique<av_wrapper::Demuxer_v3>();

    demuxer->register_video_callback([](const int64_t pakcet_idx, const std::shared_ptr<AVPacket> &packet) {
        if (pakcet_idx % 200 == 0) { printf("=================================== packet count: %ld\n", pakcet_idx); }
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        return true;
    });

    demuxer->open_stream(input_url,
                         av_wrapper::DemuxerOptions{.tcp_transport = true, .jump_first_video_i_frame = true});

    std::this_thread::sleep_for(std::chrono::seconds(10));
}

int main(int argc, char **argv) {

    for (int i = 0; i < 20; i++) {
        std::thread thread_handle[16];
        for (int j = 0; j < 16; j++) { thread_handle[j] = std::thread(running, argv[1]); }
        for (int j = 0; j < 16; j++) {
            if (thread_handle[j].joinable()) { thread_handle[j].join(); }
        }
    }

    return 0;
}