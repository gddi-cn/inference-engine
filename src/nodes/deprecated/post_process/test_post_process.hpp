#ifndef TEST_POST_PROCESS_
#define TEST_POST_PROCESS_

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>

namespace gddi {
namespace nodes {

class TestFileInput : public node_any_basic<TestFileInput> {
protected:
    std::string input_dir_;
    message_pipe<msgs::cv_frame> output_result_;
    gddi::MemPool<cv::Mat, int, int> mem_pool_;

public:
    explicit TestFileInput(std::string name) : node_any_basic<TestFileInput>(std::move(name)) {
        bind_simple_property("input_dir", input_dir_, "输入目录");
        output_result_ = register_output_message_<msgs::cv_frame>();
    }

    ~TestFileInput() override = default;

private:
    void on_setup() {
        std::cout << "===> read file: " << input_dir_ << std::endl;
        auto mem_obj = mem_pool_.alloc_mem_detach(0, 0);
        *mem_obj->data = cv::imread(input_dir_);
        auto img_data = std::make_shared<msgs::cv_frame>("test");
        img_data->frame_info = std::make_shared<gddi::nodes::FrameInfo>(0, mem_obj);
        output_result_(img_data);
    }
};

class CropImage : public node_any_basic<CropImage> {
protected:
    std::string save_path_;
    int x1_;
    int y1_;
    int x2_;
    int y2_;
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit CropImage(std::string name) : node_any_basic<CropImage>(std::move(name)) {
        bind_simple_property("save_path", save_path_, "保存路径");
        bind_simple_property("x1", x1_, "左上 x 座标");
        bind_simple_property("y1", y1_, "左上 Y 座标");
        bind_simple_property("x2", x2_, "右下 x 座标");
        bind_simple_property("y2", y2_, "右下 y 座标");
        register_input_message_handler_(&CropImage::on_frame, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }

    ~CropImage() override = default;

private:
    void on_setup() { std::cout << "CropImage node" << std::endl; }

    void on_frame(const std::shared_ptr<msgs::cv_frame> &frame) {

        std::cout << "CropImg: on_frame" << std::endl;
        cv::Mat img = *frame->frame_info->src_frame->data;
        cv::Rect crop_region(x1_, y1_, x2_, y2_);
        cv::Mat crop_img = img(crop_region);
        cv::imwrite(save_path_, crop_img);

        output_result_(frame);
    }
};
}// namespace nodes
}// namespace gddi

#endif
