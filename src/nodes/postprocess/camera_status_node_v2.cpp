#include "camera_status_node_v2.h"

#if defined(WITH_NVIDIA)
#include <opencv2/cudaarithm.hpp>
#endif

namespace gddi {
namespace nodes {

void CameraStatus_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
// #if defined(WITH_BM1684)
//     cv::Mat src_frame;
//     cv::bmcv::toMAT(frame->frame_info->src_frame->data.get(), src_frame);
//     src_frame = src_frame.clone();
// #elif defined(WITH_MLU220)
//     auto src_frame = image_wrapper::image_to_mat(frame->frame_info->src_frame->data);
//     src_frame = src_frame(cv::Rect(src_frame.cols / 4, src_frame.rows / 4, src_frame.cols / 2, src_frame.rows / 2));
// #endif

//     // 初始化第一帧
//     if (prev_frame_.empty()) {
// #if defined(WITH_BM1684) || defined(WITH_MLU220)
//         prev_frame_ = src_frame;
// #elif defined(WITH_NVIDIA)
//         prev_frame_ = *frame->frame_info->src_frame->data;
// #endif
//         return;
//     }

//     // 每隔 staing_interval_ 秒，检查一次
//     if (time(NULL) - prev_moving_time_ > staing_interval_) {
// #if defined(WITH_BM1684) || defined(WITH_MLU220)
//         cv::Mat diff;
//         cv::absdiff(prev_frame_, src_frame, diff);
//         int mean = cv::mean(diff)[0];
// #elif defined(WITH_NVIDIA)
//         cv::cuda::GpuMat diff;
//         cv::cuda::absdiff(prev_frame_, *frame->frame_info->src_frame->data, diff);
//         std::vector<int> meam;
//         cv::cuda::meanStdDev(diff, meam);
//         int mean = meam[0];
// #else
//         cv::Mat src_frame = image_wrapper::yuv420p_to_bgr888(*frame->frame_info->src_frame->data);
//         cv::Mat diff;
//         cv::absdiff(prev_frame_, src_frame, diff);
//         int mean = cv::mean(diff)[0];
// #endif

//         // 有移动
//         if (mean > moving_threshold_) {
//             prev_status_ = CameraStatus::kMoving;
//             prev_moving_time_ = time(NULL);
//         } else if (prev_status_ == CameraStatus::kMoving && time(NULL) - prev_moving_time_ > staing_time_) {
//             prev_status_ = CameraStatus::kStaing;
//             prev_moving_time_ = time(NULL);
//             frame->check_report_callback_ = [](const std::vector<FrameExtInfo> &) { return FrameType::kDisappear; };
//         }

//         prev_frame_ = src_frame;
//     }

    output_result_(frame);
}

}// namespace nodes
}// namespace gddi
