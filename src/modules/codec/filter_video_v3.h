#ifndef __FILTER_VIDEO_H__
#define __FILTER_VIDEO_H__

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}
#include <map>
#include <memory>
#include <functional>

namespace av_wrapper {

class FilterPrivate;
using FilterCallback = std::function<void(const int64_t, const std::shared_ptr<AVFrame> &)>;

class Filter_v3 {

public:
    Filter_v3();
    Filter_v3(const Filter_v3 &) = delete;           // No Copy
    Filter_v3 &operator=(const Filter_v3 &) = delete;// No Copy
    ~Filter_v3();

    void register_filter_callback(const FilterCallback &filter_cb);

    bool init_filter(const std::shared_ptr<AVCodecParameters> &codecpar);
    bool filter_frame(const int64_t frame_idx, const std::shared_ptr<AVFrame> &frame);

private:
    std::unique_ptr<FilterPrivate> impl_;
    FilterCallback filter_cb_;
};
}// namespace av_wrapper

#endif