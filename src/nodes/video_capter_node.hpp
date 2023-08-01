// create by jiaxinwang on 2021/12/02

#pragma once
#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include <chrono>
#include <opencv2/core.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <thread>
namespace gddi {
namespace nodes {
class VideoCaptureNode : public node_any_basic<VideoCaptureNode> {
protected:
    std::string video_path_;
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit VideoCaptureNode(std::string name) : node_any_basic<VideoCaptureNode>(std::move(name)) {
        bind_simple_property("video_path", video_path_, "视频文件");
        output_result_ = register_output_message_<msgs::cv_frame>();
    }
    ~VideoCaptureNode() override = default;

private:
    void on_setup() {

#ifdef WITH_BM1684
        mem_pool_.register_free_callback([](bm_image *ptr) { bm_image_destroy(*ptr); });
#endif

        std::cout << "===> read video: " << video_path_ << std::endl;
        cv::VideoCapture cap;
        cap.open(video_path_);

        if (!cap.isOpened()) {
            std::cout << "cannot open video file " << video_path_ << std::endl;
            assert(false);
        }

        int frame_id = 0;
        while (true) {
            cv::Mat img;
            cap.read(img);
            if (img.empty()) break;

            auto frame = std::make_shared<msgs::cv_frame>("", gddi::TaskType::kCamera, cap.get(cv::CAP_PROP_FPS));
#if defined(WITH_BM1684)
            auto mem_obj = mem_pool_.alloc_mem_detach<std::shared_ptr<AVFrame>>(nullptr, FORMAT_RGB_PACKED, img.cols,
                                                                                img.rows, false);
            if (cv::bmcv::toBMI(img, mem_obj->data.get()) != BM_SUCCESS) {
                throw std::runtime_error("Failed to convert bm_image");
            }
#elif defined(WITH_MLU220) || defined(WITH_MLU270)
            auto mem_obj =
                mem_pool_.alloc_mem_detach<std::shared_ptr<AVFrame>>(nullptr, CNCODEC_PIX_FMT_NV12, img.cols, img.rows);
#elif defined(WITH_NVIDIA)
            auto mem_obj = mem_pool_.alloc_mem_detach(img.cols, img.rows);
            mem_obj->data->upload(img);
#elif defined(WITH_MLU370)
            auto mem_obj = mem_pool_.alloc_mem_detach();
#else
            auto mem_obj = mem_pool_.alloc_mem_detach(img.cols, img.rows);
#endif
            frame->frame_info = std::make_shared<nodes::FrameInfo>(frame_id, mem_obj);
            output_result_(frame);

            std::cout << "read frame " << frame_id << std::endl;
            frame_id++;

            if (frame_id > 10) break;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    };

private:
    ImagePool mem_pool_;
};
}// namespace nodes
}// namespace gddi
