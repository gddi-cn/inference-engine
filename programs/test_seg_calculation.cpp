#include "modules/postprocess/seg_area.h"
#include "node_msg_def.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <opencv2/imgproc.hpp>
#include <vector>

int main() {

    cv::Mat camera_matrix = (cv::Mat_<float>(3, 3) << 1148.0678727231252, 0.0, 968.0883806940227,
                             0.0, 1155.8458179063227, 539.3126117576774, 0.0, 0.0, 1.0);
    cv::Mat dist_coeffs = (cv::Mat_<float>(5, 1) << -0.42022691705122023, 0.2110163003990064,
                           -0.0005615454442203457, -0.00034488052319524005, -0.05416083319699509);
    cv::Mat r_vec =
        (cv::Mat_<float>(3, 1) << -0.7844210658650189, -0.7820100265658598, -1.457766991763434);
    cv::Mat t_vec =
        (cv::Mat_<float>(3, 1) << -16.647841493742657, 9.682050266632663, 225.67105129376233);
    cv::Mat p_matrix = (cv::Mat_<float>(3, 3) << 38.16695850338316, 235.94419968765757,
                        -32813.03199289429, 4.488205971023693, 277.0548181911571,
                        -46775.546849139835, 0.003207917543062167, 0.2651306464357172, 1.0);
    std::vector<uint8_t> vec_labels{1};

    cv::Mat output_mask;
    cv::Mat input_mask = cv::imread("bev_test_mask_label.png", cv::IMREAD_UNCHANGED);
    cv::resize(input_mask, input_mask, cv::Size(1920, 1080), cv::INTER_NEAREST);

    auto bev_trans = std::make_unique<gddi::BevTransform>();
    bev_trans->SetParam(input_mask.cols, input_mask.rows, camera_matrix, dist_coeffs, r_vec, t_vec,
                        p_matrix);
    auto seg_by_contours = std::make_unique<gddi::SegAreaAndMaxLengthByContours>();

    // {
    //     int rows = input_mask.rows;
    //     int cols = input_mask.cols;
    //     uint8_t *data = input_mask.data;
    //     auto start = std::chrono::steady_clock::now();

    //     for (int i = 0; i < rows; i++) {
    //         for (int j = 0; j < cols; j++) {
    //             *(data + i * (cols - 1) + j) = 1;
    //         }
    //     }
    //     printf("====================== %d\n",
    //            std::chrono::duration_cast<std::chrono::microseconds>(
    //                std::chrono::steady_clock::now() - start)
    //                .count());
    // }

    auto start = std::chrono::steady_clock::now();
    bev_trans->PostProcess(input_mask, &output_mask);

    std::map<uint8_t, std::vector<gddi::nodes::SegContour>> seg_info;
    seg_by_contours->PostProcess(output_mask, vec_labels, seg_info);
    auto time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();

    for (const auto &[idx, contours] : seg_info) {
        for (const auto &item : contours) {
            printf("================ idx: %d, area: %.6f, length: %.6f, time: %ldms\n", idx,
                   item.area, item.length, time);
        }
    }

    return 0;
}