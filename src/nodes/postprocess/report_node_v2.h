//
// Created by zhdotcai on 2022/3/8.
//

#ifndef __REPORT_NODE_V2_HPP__
#define __REPORT_NODE_V2_HPP__

#include <condition_variable>
#include <future>
#include <memory>
#include <queue>

#include "message_templates.hpp"
#include "modules/codec/encode_video_v3.h"
#include "modules/codec/remux_stream_v3.h"
#include "modules/wrapper/draw_image.h"
#include "modules/wrapper/export_video.h"
#include "modules/wrapper/tsing_jpeg_encode.h"
#include "node_any_basic.hpp"
#include "node_msg_def.h"

namespace gddi {
namespace nodes {

class Report_v2 : public node_any_basic<Report_v2> {
public:
    explicit Report_v2(std::string name) : node_any_basic<Report_v2>(std::move(name)) {
        bind_simple_property("report_url", report_url_, "服务地址");
        bind_simple_property("time_interval", time_interval_, "时间间隔");
        bind_simple_property("image_quality", image_quality_, "图像编码质量(1-100)");
        bind_simple_property("real_time_push", real_time_push_, "实时推送");
        bind_simple_property("codec_type", codec_type_, "编码类型");
        bind_simple_property("save_time", save_time_, "编码时长");

        bind_simple_property("task_name", task_name_, ngraph::PropAccess::kPrivate);

        register_input_message_handler_(&Report_v2::on_report, this);
    }

private:
    void on_setup() override;
    void on_report(const std::shared_ptr<msgs::cv_frame> &frame);

    std::string frame_info_to_string(const std::string &task_name, const std::string &event_id,
                                     const int64_t &event_time, const std::shared_ptr<nodes::FrameInfo> &frame_info);

    void write_frame(cv::Mat &image, const int framerate);

    void draw_image(const int frame_rate, const std::shared_ptr<nodes::FrameInfo> &frame_info);

private:
    std::queue<std::pair<std::string, std::string>> que_taskid_record_;
    std::string report_url_;         // 服务地址
    uint32_t time_interval_{0};      // 时间间隔
    uint32_t image_quality_{85};     // 编码质量
    bool real_time_push_{false};     // 实时推送标识
    std::string codec_type_{"mjpeg"};// 编码类型
    uint32_t save_time_{15};         // 保存时长

    time_t last_event_time_;// 最后一次上报时间

    std::string task_name_;
    std::string event_folder_;

    std::unique_ptr<codec::TsingJpegEncode> jpeg_decoder_;

    std::vector<std::shared_ptr<nodes::FrameInfo>> cache_frames_;

    std::unique_ptr<DrawImage> draw_image_;
    std::unique_ptr<ExportVideo> export_video_;

    std::future<void> async_result_;
};
}// namespace nodes
}// namespace gddi

#endif//__REPORT_NODE_V2_HPP__
