//
// Created by cc on 2021/11/15.
//

#ifndef __IMAGE_NODE_V2_HPP__
#define __IMAGE_NODE_V2_HPP__

#include <hv/HttpServer.h>
#include <map>
#include <memory>
#include <opencv2/core/hal/interface.h>

#include "message_templates.hpp"
#include "modules/network/downloader.h"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"

namespace gddi {
namespace nodes {

enum class ImageFormat { kJPEG = 0, kPNG, kNV12, kBGR, kNone };

class ImageServer_v2 : public node_any_basic<ImageServer_v2> {
private:
    message_pipe<msgs::cv_frame> output_image_;

public:
    explicit ImageServer_v2(std::string name) : node_any_basic(std::move(name)) {
        output_image_ = register_output_message_<msgs::cv_frame>();

        bind_simple_property("task_name", task_name_, ngraph::PropAccess::kPrivate);
        bind_simple_property("srv_port", srv_port_, ngraph::PropAccess::kProtected);
        bind_simple_property("scale_factor", scale_factor_, "缩放系数");

        register_input_message_handler_(&ImageServer_v2::on_response, this);
    }

    ~ImageServer_v2();

protected:
    void on_setup() override;

private:
    void on_request(const HttpContextPtr &ctx);
    void on_construct(const std::vector<uchar> &img_data, const nlohmann::json &additional, const HttpContextPtr &ctx);
    void on_response(const std::shared_ptr<msgs::cv_frame> &request);

private:
    std::string task_name_;
    int srv_port_;
    float scale_factor_{1.5};

    int64_t frame_idx_{0};

    HttpService router_;
    http_server_t server_;

#if defined(WITH_NVIDIA)
    ImagePool mem_pool_;
    std::unique_ptr<wrapper::NvJpegDecoder> jpeg_decoder_;
#else
    ImagePool mem_pool_;
#endif
    std::unique_ptr<network::Downloader> downloader_;
};
}// namespace nodes

}// namespace gddi

#endif//__IMAGE_NODE_V2_HPP__
