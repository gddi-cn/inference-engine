#include "filter_video_v3.h"
#include <stdexcept>

namespace av_wrapper {

class FilterPrivate {
public:
    FilterPrivate() { filter_graph = avfilter_graph_alloc(); }
    ~FilterPrivate() { avfilter_graph_free(&filter_graph); }

    AVFilterGraph *filter_graph = nullptr;
    AVFilterContext *buffer_ctx = nullptr;
    AVFilterContext *buffersink_ctx = nullptr;
};

Filter_v3::Filter_v3() { impl_ = std::make_unique<FilterPrivate>(); }
Filter_v3::~Filter_v3() {}

void Filter_v3::register_filter_callback(const FilterCallback &filter_cb) {
    filter_cb_ = filter_cb;
}

bool Filter_v3::init_filter(const std::shared_ptr<AVCodecParameters> &codecpar) {
    auto buffer = avfilter_get_by_name("buffer");
    if (!buffer) { throw std::runtime_error("Could not find the abuffer filter"); }

    impl_->buffer_ctx = avfilter_graph_alloc_filter(impl_->filter_graph, buffer, "in");
    if (!impl_->buffer_ctx) { throw std::runtime_error("Could not allocate the abuffer instance"); }

    char ch_layout[64];
    snprintf(ch_layout, sizeof(ch_layout), "%dx%d", 1920, 1080);
    av_opt_set(impl_->buffer_ctx, "video_size", ch_layout, AV_OPT_SEARCH_CHILDREN);
    av_opt_set_int(impl_->buffer_ctx, "pix_fmt", AV_PIX_FMT_YUV420P, AV_OPT_SEARCH_CHILDREN);
    av_opt_set_q(impl_->buffer_ctx, "time_base", (AVRational){1, 25}, AV_OPT_SEARCH_CHILDREN);
    av_opt_set_q(impl_->buffer_ctx, "pixel_aspect", codecpar->sample_aspect_ratio, AV_OPT_SEARCH_CHILDREN);
    // av_opt_set_int(impl_->buffer_ctx, "extra_hw_frames", 3, AV_OPT_SEARCH_CHILDREN);

    if (avfilter_init_str(impl_->buffer_ctx, nullptr) < 0) {
        throw std::runtime_error("Could not initialize the abuffer filter");
    }

    auto scale = avfilter_get_by_name("scale");
    if (!scale) { throw std::runtime_error("Could not find the scale filter"); }

    auto scale_ctx = avfilter_graph_alloc_filter(impl_->filter_graph, scale, "scale_0");
    if (!scale_ctx) { throw std::runtime_error("Could not allocate the scale instance"); }

    AVDictionary *options_dict = nullptr;
    av_dict_set(&options_dict, "scale", "1920:1080", 0);
    if (avfilter_init_dict(scale_ctx, &options_dict) < 0) {
        av_dict_free(&options_dict);
        throw std::runtime_error("Could not initialize the scale_npp filter");
    }
    av_dict_free(&options_dict);

    auto buffersink = avfilter_get_by_name("buffersink");
    if (!buffersink) { throw std::runtime_error("Could not find the buffersink filter"); }

    impl_->buffersink_ctx = avfilter_graph_alloc_filter(impl_->filter_graph, buffersink, "out");
    if (!impl_->buffersink_ctx) {
        throw std::runtime_error("Could not allocate the buffersink instance");
    }

    if (avfilter_init_str(impl_->buffersink_ctx, nullptr) < 0) {
        throw std::runtime_error("Could not initialize the buffersink instance");
    }

    if (avfilter_link(impl_->buffer_ctx, 0, scale_ctx, 0) < 0
        || !avfilter_link(scale_ctx, 0, impl_->buffersink_ctx, 0) < 0) {
        throw std::runtime_error("Error connecting filters");
    }

    if (avfilter_graph_config(impl_->filter_graph, nullptr) < 0) {
        throw std::runtime_error("Error configuring the filter graph");
    }

    return true;
}

bool Filter_v3::filter_frame(const int64_t frame_idx, const std::shared_ptr<AVFrame> &frame) {
    if (av_buffersrc_add_frame(impl_->buffer_ctx, frame.get()) < 0) {
        throw std::runtime_error("Error submitting the frame to the filtergraph");
    }

    auto sink_frame =
        std::shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *ptr) { av_frame_free(&ptr); });
    if (av_buffersink_get_frame(impl_->buffersink_ctx, sink_frame.get()) >= 0) {
        if (filter_cb_) { filter_cb_(frame_idx, sink_frame); }
        return true;
    }

    return false;
}

}// namespace av_wrapper