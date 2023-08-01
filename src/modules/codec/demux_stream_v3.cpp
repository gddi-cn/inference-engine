#include "demux_stream_v3.h"
#include "basic_logs.hpp"
#include "thread.hpp"
#include <algorithm>
#include <mutex>

extern "C" {
#include <libavutil/opt.h>
#include <libavutil/time.h>
}

namespace av_wrapper {

struct OpaqueOptions {
    bool demux_on;
    std::chrono::steady_clock::time_point time_point;
};

static inline bool is_video_file(const std::string &stream_url) {
    if (stream_url.size() > 4) {
        auto subs = stream_url.substr(0, 4);
        std::transform(subs.begin(), subs.end(), subs.begin(), ::tolower);
        std::transform(subs.begin(), subs.end(), subs.begin(), [](unsigned char c) { return std::tolower(c); });
        if (subs == "file" || subs == "http") { return true; }
    }
    return false;
}

class DemuxerPrivate {
public:
    DemuxerPrivate(const Demuxer_v3::OpenCallback &open, const Demuxer_v3::PacketCallback &read,
                   const Demuxer_v3::CloseCallback &exit)
        : opaque({true}), open_cb(open), packet_cb(read), exit_cb(exit) {}
    ~DemuxerPrivate() {
        opaque.demux_on = false;
        thread_handle.reset();
        if (fmt_ctx_) {
            avformat_close_input(&fmt_ctx_);
            fmt_ctx_ = nullptr;
        }
        av_dict_free(&dicts);
    }

    void open_stream(const std::string stream_url, const DemuxerOptions &options);

    friend class Demuxer_v3;

protected:
    void open_stream_impl();
    void read_stream_packet(const Demuxer_v3::PacketCallback &packet_cb);
    void dump_demuxer_stat();

    const AVInputFormat *get_input_format() {
        if (fmt_ctx_) return fmt_ctx_->iformat;
        return nullptr;
    }

    double get_video_frame_rate() {
        if (fmt_ctx_ && video_stream_index >= 0) return av_q2d(fmt_ctx_->streams[video_stream_index]->r_frame_rate);
        return 0;
    }

private:
    AVFormatContext *fmt_ctx_{nullptr};
    std::unique_ptr<gddi::Thread> thread_handle{nullptr};

    Demuxer_v3::OpenCallback open_cb{nullptr};
    Demuxer_v3::PacketCallback packet_cb{nullptr};
    Demuxer_v3::CloseCallback exit_cb{nullptr};

    std::string stream_url_;
    DemuxerOptions options_;

    OpaqueOptions opaque;
    AVDictionary *dicts{nullptr};

    int video_stream_index{-1};

    int64_t pkt_count{0};              // 读取包计数
    int64_t pkt_video_count{0};        // 视频帧统计
    int64_t pkt_video_i_frame_count{0};// 视频I-Frame帧统计
};

void DemuxerPrivate::open_stream(const std::string stream_url, const DemuxerOptions &options) {
    stream_url_ = stream_url;
    options_ = options;

    thread_handle = std::make_unique<gddi::Thread>(
        [this]() {
            while (opaque.demux_on) {
                try {
                    open_stream_impl();
                    if (open_cb) {
                        auto codecpar = std::shared_ptr<AVCodecParameters>(
                            avcodec_parameters_alloc(), [](AVCodecParameters *ptr) { avcodec_parameters_free(&ptr); });
                        avcodec_parameters_copy(codecpar.get(), fmt_ctx_->streams[video_stream_index]->codecpar);
                        open_cb(codecpar);
                    }
                    read_stream_packet(packet_cb);
                    spdlog::info("Stream exit: {}, packeds: {}", stream_url_, pkt_count);
                } catch (const std::exception &e) {
                    spdlog::error(e.what());
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                    if (!opaque.demux_on && exit_cb) {
                        exit_cb(false);
                        exit_cb = nullptr;
                    }
                }
                if (is_video_file(stream_url_)) break;
            }
            if (exit_cb) { exit_cb(true); }
        },
        gddi::Thread::DtorAction::join);
    thread_handle->start();
}

void DemuxerPrivate::open_stream_impl() {
    if (fmt_ctx_) { avformat_close_input(&fmt_ctx_); }
    fmt_ctx_ = avformat_alloc_context();

    stream_url_.erase(std::remove_if(stream_url_.begin(), stream_url_.end(),
                                     [](char x) -> bool { return x == ' ' || x == '\t' || x == '\r' || x == '\n'; }),
                      stream_url_.end());

    if (options_.tcp_transport) {
        av_dict_set(&dicts, "rtsp_transport", "tcp", 0);
        av_dict_set(&dicts, "stimeout", "3000000", 0);
    } else {
        av_dict_set(&dicts, "timeout", "3000000", 0);
    }

#ifdef BM_PCIE_MODE
    av_dict_set_int(&dicts, "zero_copy", pcie_no_copyback, 0);
    av_dict_set_int(&dicts, "sophon_idx", sophon_idx, 0);
#endif

    // av_dict_set(&dicts, "buffer_size", "1024000", 0);
    av_opt_set_int(fmt_ctx_, "max_delay", 5000000, AV_OPT_SEARCH_CHILDREN);

    opaque.time_point = std::chrono::steady_clock::now();
    /*-------------- set callback, avoid blocking --------------*/
    fmt_ctx_->interrupt_callback.callback = [](void *opaque) {
        OpaqueOptions *p_opaque = (OpaqueOptions *)opaque;
        if (!p_opaque->demux_on
            || std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - p_opaque->time_point)
                    .count()
                > 3) {
            return 1;
        }
        return 0;
    };
    fmt_ctx_->interrupt_callback.opaque = &opaque;
    /*--------------------------- end --------------------------*/

    if (avformat_open_input(&fmt_ctx_, stream_url_.c_str(), nullptr, &dicts) != 0) {
        throw std::runtime_error("Couldn't open input stream: " + stream_url_);
    }

    // read packets of a media file to get stream information
    if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0) {
        throw std::runtime_error("Couldn't find stream information: " + stream_url_);
    }

    // find the "best" stream in the file
    video_stream_index = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index < 0) { throw std::runtime_error("Didn't find a video stream: " + stream_url_); }

    // dump stream info
    av_dump_format(fmt_ctx_, video_stream_index, stream_url_.c_str(), 0);
}

void DemuxerPrivate::read_stream_packet(const Demuxer_v3::PacketCallback &packet_cb) {
    int64_t last_pts = AV_NOPTS_VALUE;

    while (opaque.demux_on) {
        auto packet = std::shared_ptr<AVPacket>(av_packet_alloc(), [](AVPacket *ptr) { av_packet_free(&ptr); });
        if (av_read_frame(fmt_ctx_, packet.get()) == 0) {
            opaque.time_point = std::chrono::steady_clock::now();

            ++pkt_count;

            // is video stream
            if (packet->stream_index == video_stream_index) {
                if (packet->flags & AV_PKT_FLAG_KEY) { ++pkt_video_i_frame_count; }

                // Jump first I-Frame & follow P-frame
                if (options_.jump_first_video_i_frame && pkt_video_i_frame_count < 1) { continue; }

                if (packet_cb) {
                    if (!packet_cb(++pkt_video_count, packet)) { break; }
                }

                if (is_video_file(stream_url_)) {
                    if (packet->pts < 0) {
                        av_usleep(AV_TIME_BASE / av_q2d(fmt_ctx_->streams[video_stream_index]->r_frame_rate) - 1000);
                    } else if (last_pts != AV_NOPTS_VALUE) {
                        int64_t delay =
                            av_rescale_q(packet->pts - last_pts, fmt_ctx_->streams[video_stream_index]->time_base,
                                         {1, AV_TIME_BASE});
                        av_usleep(delay);
                    }
                    last_pts = packet->pts;
                }
            }
        } else {
            // < 0 on error or end of file
            break;
        }
    }
}

void DemuxerPrivate::dump_demuxer_stat() {
    spdlog::info("total packet: {}", pkt_count);
    spdlog::info("video packet: {}", pkt_video_count);
    spdlog::info("video i packet: {}", pkt_video_i_frame_count);
}

//################################## Demuxer_v3 Begin ##################################

Demuxer_v3::Demuxer_v3() { av_log_set_level(AV_LOG_ERROR); }
Demuxer_v3::~Demuxer_v3() {}

Demuxer_v3::Demuxer_v3(Demuxer_v3 &&other) {
    open_cb_ = std::move(other.open_cb_);
    video_cb_ = std::move(other.video_cb_);
    close_cb_ = std::move(other.close_cb_);
    impl_ = std::move(other.impl_);
}

void Demuxer_v3::open_stream(const std::string &stream_url, const DemuxerOptions &options) {
    impl_ = std::make_unique<DemuxerPrivate>(open_cb_, video_cb_, close_cb_);
    impl_->open_stream(stream_url, options);
}

void Demuxer_v3::close_stream() { impl_.reset(); }

void Demuxer_v3::register_open_callback(const OpenCallback &open_cb) { open_cb_ = open_cb; }
void Demuxer_v3::register_video_callback(const PacketCallback &packet_cb) { video_cb_ = packet_cb; }
void Demuxer_v3::register_audio_callback(const PacketCallback &packet_cb) {}
void Demuxer_v3::register_close_callback(const CloseCallback &close_cb) { close_cb_ = close_cb; }

double Demuxer_v3::get_video_frame_rate() {
    if (impl_) return impl_->get_video_frame_rate();
    return 0;
}
//################################### Demuxer_v3 End ###################################

}// namespace av_wrapper