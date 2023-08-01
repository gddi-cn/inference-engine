#ifndef __DEMUXER_V3_H__
#define __DEMUXER_V3_H__

extern "C" {
#include <libavformat/avformat.h>
}
#include <boost/noncopyable.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace av_wrapper {

class DemuxerPrivate;

struct DemuxerOptions {
    bool tcp_transport{true};// 使用 tcp 协议，默认 true
    // 是否跳过第一个I-Frame，默认 true, 第一个I-Frame貌似会pts错误
    bool jump_first_video_i_frame{true};
};

class Demuxer_v3 : public boost::noncopyable {
public:
    /**
     * @brief 打开时回调
     * 
     */
    using OpenCallback = std::function<void(const std::shared_ptr<AVCodecParameters> &)>;

    /**
     * @brief 压缩包回调
     * 
     */
    using PacketCallback = std::function<bool(const int64_t, const std::shared_ptr<AVPacket> &)>;

    /**
     * @brief 关闭时回调
     * 
     */
    using CloseCallback = std::function<void(const bool)>;

public:
    Demuxer_v3();
    ~Demuxer_v3();

    Demuxer_v3(Demuxer_v3 &&demuxer);// Movable

    /**
     * @brief 打开输入流
     * 
     * @param stream_url 
     * @param options 
     */
    void open_stream(const std::string &stream_url, const DemuxerOptions &options = {});

    /**
     * @brief 关闭视音解包
     * 
     */
    void close_stream();

    /**
     * @brief 获取视频流帧率
     * 
     * @return double 
     */
    double get_video_frame_rate();

    /**
     * @brief 注册打开回调
     * 
     * @param cb 
     */
    void register_open_callback(const OpenCallback &open_cb);

    /**
     * @brief 注册视频压缩包回调
     * 
     * @param cb 
     */
    void register_video_callback(const PacketCallback &video_cb);

    /**
     * @brief 注册音频压缩包回调
     * 
     * @param cb 
     */
    void register_audio_callback(const PacketCallback &audio_cb);

    /**
     * @brief 注册关闭回调
     * 
     * @param cb 
     */
    void register_close_callback(const CloseCallback &close_cb);

    /**
     * @brief 打印输入流信息
     * 
     * @param oss 
     */
    void dump_demuxer_stat(std::ostream &oss);

private:
    OpenCallback open_cb_{nullptr};
    PacketCallback video_cb_{nullptr};
    PacketCallback audio_cb_{nullptr};
    CloseCallback close_cb_{nullptr};

    std::unique_ptr<DemuxerPrivate> impl_;
};

}// namespace av_wrapper

#endif//__DEMUXER_V3_H__