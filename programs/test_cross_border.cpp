/**
 * @file test_cross_border.cpp
 * @author zhdotcai
 * @brief 
 * @version 0.1
 * @date 2022-12-06
 * 
 * @copyright Copyright (c) 2022 GDDI
 * 
 */

#include "modules/bytetrack/target_tracker.h"
#include "modules/codec/decode_video_v3.h"
#include "modules/codec/demux_stream_v3.h"
#include "modules/postprocess/cross_border.h"
#include "utils.hpp"
#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include <memory>
#include <opencv2/core/cuda.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <vector>

AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_NONE;

void running(const std::string &input_url, const std::string model_url) {
    // 解码
    auto demuxer = std::make_shared<av_wrapper::Demuxer_v3>();
    auto decoder = std::make_shared<av_wrapper::Decoder_v3>();

    // 跟踪 + 越界
    auto tracker = std::make_shared<gddi::TargetTracker>(
        gddi::TrackOption{.track_thresh = 0.5, .high_thresh = 0.6, .match_thresh = 0.8, .max_frame_lost = 25});
    auto counter = std::make_shared<gddi::CrossBorder>();

    // 初始化边界, 规定从左到右穿过射线视为越界
    std::vector<gddi::Point2i> border{{960, 1080}, {960, 0}};
    counter->init_border(border);

    decoder->register_deocde_callback(
        [&tracker, &counter](const int64_t frame_idx, const std::shared_ptr<AVFrame> &avframe) {
            // 存放目标信息
            auto vec_objects = std::vector<gddi::TrackObject>();

            // 目标跟踪
            std::map<int, gddi::Rect2f> rects;
            auto tracked_target = tracker->update_objects(vec_objects);
            for (auto &[track_id, target] : tracked_target) {
                rects[track_id] = gddi::Rect2f{target.rect.x, target.rect.y, target.rect.width, target.rect.height};
                // printf("track_id: %d, x: %d, y: %d, width: %d, height: %d\n", track_id, target.rect.x,
                //        target.rect.y, target.rect.width, target.rect.height);
            }

            // 更新目标位置，若 vec_direction 非空，则有新目标越界
            auto vec_direction = counter->update_position(rects);
            for (const auto [track_id, direction] : vec_direction[0]) {
                printf("track_id: %d, direction: %d\n", track_id, direction);
            }

            return true;
        });

    demuxer->register_open_callback(
        [decoder](const std::shared_ptr<AVCodecParameters> &codecpar) { decoder->open_decoder(codecpar, hw_type); });

    demuxer->register_video_callback([decoder](const int64_t pakcet_idx, const std::shared_ptr<AVPacket> &packet) {
        return decoder->decode_packet(packet);
    });

    demuxer->open_stream(input_url,
                         av_wrapper::DemuxerOptions{.tcp_transport = true, .jump_first_video_i_frame = true});

    std::this_thread::sleep_for(std::chrono::seconds(3600));
}

void usage() {
    printf("Options:\n");
    printf(" -s, --stream_url=        Video stream url.\n");
    printf(" -m, --model_url=         Model file path\n");
    printf(" -t, --threads=1          The number of threads\n");
}

int main(int argc, char **argv) {
    static struct option long_options[] = {{"stream_url", required_argument, 0, 's'},
                                           {"model_url", required_argument, 0, 'm'},
                                           {"threads", no_argument, 0, 't'},
                                           {"help", no_argument, NULL, 'h'}};

    size_t thread_num{1};
    std::string stream_url;
    std::string model_url;

    while (true) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "smt", long_options, &option_index);
        if (c == -1) break;

        switch (c) {
            case 's': stream_url = optarg; break;
            case 'm': model_url = optarg; break;
            case 't': thread_num = atoi(optarg); break;
            case 'h': usage(); return 1;
            default: printf("?? getopt returned character code 0%o ??\n", c);
        }
    }

    printf("stream_url: %s, model_url: %s\n", stream_url.c_str(), model_url.c_str());

    std::thread thread_handle[thread_num];
    for (int j = 0; j < thread_num; j++) { thread_handle[j] = std::thread(running, stream_url, model_url); }
    for (int j = 0; j < thread_num; j++) {
        if (thread_handle[j].joinable()) { thread_handle[j].join(); }
    }

    return 0;
}