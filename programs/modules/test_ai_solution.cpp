#include "modules/dahuatech/ai_solution.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <thread>

int main() {
    cv::Mat yuv420;
    cv::Mat bgr_image = cv::imread("jiwei_1920x1080.jpg");
    cv::cvtColor(bgr_image, yuv420, cv::COLOR_BGR2YUV_I420);

    std::vector<cv::Mat> channels(3);
    int width = bgr_image.cols;
    int height = bgr_image.rows;
    channels[0] = yuv420(cv::Rect(0, 0, width, height));                     // Y通道
    channels[1] = yuv420(cv::Rect(0, height, width / 2, height / 2));        // U通道
    channels[2] = yuv420(cv::Rect(width / 2, height, width / 2, height / 2));// V通道

    // 合并UV通道以生成NV12格式
    cv::Mat uv;
    cv::merge(std::vector<cv::Mat>{channels[1], channels[2]}, uv);

    // 将Y和UV通道合并为一个Mat，生成NV12格式
    auto nv12 = cv::Mat(height + height / 2, width, CV_8UC1);
    channels[0].copyTo(nv12(cv::Rect(0, 0, width, height)));
    for (int j = 0; j < height / 2; ++j) {
        for (int i = 0; i < width; ++i) { nv12.at<uchar>(height + j, i) = uv.at<uchar>(j, i); }
    }


#ifdef WITH_MLU220

    ai_input_s input;
    input.frames = new ai_input_frame_s;
    input.frames->frame_type = AI_INPUT_TYPE_IMAGE;
    input.frames->frame = new ai_input_image_s;

    auto image_input = reinterpret_cast<ai_input_image_s *>(input.frames->frame);
    image_input->width = width;
    image_input->height = height;
    image_input->colorspace = AI_CS_NV12;
    image_input->stride = nv12.cols;
    image_input->main_data = nv12.data;
    image_input->frame_id = 0;


    ai_verion_s version;
    ai_get_version(&version);
    printf("version: %s\n", version.version);

    ai_capacity_s caps;
    ai_get_capacities(&caps);
    printf("max_result_obj: %d\n", caps.max_result_obj);

    void *handle;
    ai_create_s create;
    create.config_file = "/home/models/100001679_0.gem";
    ai_create(&handle, &create);

    ai_register_callback(handle, AI_CB_TYPE_PROCESS_DONE, nullptr, nullptr);

    ai_process(handle, &input, 0, nullptr);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    uint32_t num;
    ai_result_s **result;
    ai_get_result(handle, &num, result);
    printf("result: %d\n", num);

    ai_release_result(handle, num, result);

    ai_destory(handle);

    ai_deinit();

#endif

    return 0;
}