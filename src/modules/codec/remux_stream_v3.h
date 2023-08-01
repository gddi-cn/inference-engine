#ifndef __REMUXE_STREAM_V3_H__
#define __REMUXE_STREAM_V3_H__

extern "C" {
#include <libavformat/avformat.h>
}

#include <map>
#include <memory>

namespace av_wrapper {

class RemuxerPrivate;

class Remuxer_v3 {
public:
    Remuxer_v3();
    ~Remuxer_v3();

    bool open_stream(const std::string &stream_url, const std::shared_ptr<AVCodecParameters> &codecpar,
                     const AVRational &timebase, const AVRational &framerate);
    bool write_packet(const std::shared_ptr<AVPacket> &packet);
    void close_stream();

private:
    std::unique_ptr<RemuxerPrivate> impl_;
};
}// namespace av_wrapper

#endif