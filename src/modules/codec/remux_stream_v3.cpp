#include "remux_stream_v3.h"
#include <memory>
#include <spdlog/spdlog.h>

namespace av_wrapper {

class RemuxerPrivate {
public:
    RemuxerPrivate() {}
    ~RemuxerPrivate() {
        if (format_dict) { av_dict_free(&format_dict); }
    }

    bool open_stream(const std::string &stream_url, const std::shared_ptr<AVCodecParameters> &codecpar,
                     const AVRational &timebase, const AVRational &framerate);
    bool write_packet(const std::shared_ptr<AVPacket> &packet);

    std::unique_ptr<AVFormatContext, void (*)(AVFormatContext *)> out_fmt_ctx_{nullptr, nullptr};
    AVStream *out_stream = nullptr;
    int64_t encoded_frames = 0;
    AVDictionary *format_dict = nullptr;
    AVRational timebase_;

    bool is_exit = false;
};

bool RemuxerPrivate::open_stream(const std::string &stream_url, const std::shared_ptr<AVCodecParameters> &codecpar,
                                 const AVRational &timebase, const AVRational &framerate) {
    timebase_ = timebase;

    try {
        AVFormatContext *fmt_ctx;
        if (stream_url.substr(0, 4) == "rtsp") {
            avformat_alloc_output_context2(&fmt_ctx, NULL, "rtsp", stream_url.c_str());
            av_dict_set(&format_dict, "rtsp_transport", "tcp", 0);
        } else if (stream_url.substr(0, 4) == "rtmp") {
            avformat_alloc_output_context2(&fmt_ctx, NULL, "flv", stream_url.c_str());
        } else {
            avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, stream_url.c_str());
        }

        /*-------------- set callback, avoid blocking --------------*/
        // out_fmt_ctx->interrupt_callback.callback = [] (void *opaque) {
        //     bool *is_exit = (bool*)opaque;
        //     if (*is_exit) {
        //         return 1;
        //     }
        //     return 0;
        // };
        // out_fmt_ctx->interrupt_callback.opaque = &impl_->is_exit;

        if (!fmt_ctx) { throw std::runtime_error("FCould not create output context"); }
        out_fmt_ctx_ = std::unique_ptr<AVFormatContext, void (*)(AVFormatContext *)>(fmt_ctx, [](AVFormatContext *ctx) {
            if (!(ctx->oformat->flags & AVFMT_NOFILE)) {
                av_write_trailer(ctx);
                avio_closep(&ctx->pb);
            }
            avformat_free_context(ctx);
        });

        if (!(out_stream = avformat_new_stream(out_fmt_ctx_.get(), nullptr))) {
            throw std::runtime_error("Failed allocating output stream");
        }

        out_stream->codecpar->codec_tag = 0;
        out_stream->time_base = timebase;
        out_stream->avg_frame_rate = framerate;
        out_stream->r_frame_rate = out_stream->avg_frame_rate;
        avcodec_parameters_copy(out_stream->codecpar, codecpar.get());

        av_dump_format(out_fmt_ctx_.get(), 0, stream_url.c_str(), 1);

        if (!(out_fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&out_fmt_ctx_->pb, stream_url.c_str(), AVIO_FLAG_WRITE) < 0) {
                throw std::runtime_error("Could not open output file " + stream_url);
            }
        }

        if (avformat_write_header(out_fmt_ctx_.get(), &format_dict) < 0) {
            throw std::runtime_error("Occurred when opening url: " + stream_url);
        }
    } catch (const std::exception &e) {
        spdlog::error(e.what());
        return false;
    }

    return true;
}

bool RemuxerPrivate::write_packet(const std::shared_ptr<AVPacket> &packet) {
    if (out_fmt_ctx_) {
        packet->stream_index = out_stream->index;
        av_packet_rescale_ts(packet.get(), timebase_, out_stream->time_base);
        return av_interleaved_write_frame(out_fmt_ctx_.get(), packet.get()) == 0;
    }
    return false;
}

Remuxer_v3::Remuxer_v3() { impl_ = std::make_unique<RemuxerPrivate>(); }
Remuxer_v3::~Remuxer_v3() {}

bool Remuxer_v3::open_stream(const std::string &stream_url, const std::shared_ptr<AVCodecParameters> &codecpar,
                             const AVRational &timebase, const AVRational &framerate) {
    return impl_->open_stream(stream_url, codecpar, timebase, framerate);
}
bool Remuxer_v3::write_packet(const std::shared_ptr<AVPacket> &packet) { return impl_->write_packet(packet); }
void Remuxer_v3::close_stream() { impl_.reset(); }

}// namespace av_wrapper