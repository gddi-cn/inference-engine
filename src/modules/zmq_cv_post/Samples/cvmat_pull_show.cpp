//
// Created by cc on 2021/12/1.
//

#include <zmq.hpp>
#include <zmq_addon.hpp>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs/legacy/constants_c.h>

int main(int argc, char *argv[]) {
    if (argc > 1) {
        std::string comm_addr = argv[1];
        zmq::context_t ctx;
        zmq::socket_t sock(ctx, zmq::socket_type::sub);
        sock.connect(comm_addr);
        sock.set(zmq::sockopt::subscribe, "");

        zmq::multipart_t mp;
        cv::namedWindow(comm_addr, cv::WINDOW_GUI_EXPANDED | cv::WINDOW_KEEPRATIO);
        while (true) {
            try {
                mp.recv(sock);

                auto data = mp.pop();
                auto labels = mp.popstr();
                auto frame_index = mp.poptyp<int64_t>();

                auto jpeg_data = std::vector<char>(data.size());
                memcpy(jpeg_data.data(), data.data(), data.size());

                cv::Mat mat = cv::imdecode(jpeg_data, CV_LOAD_IMAGE_COLOR);

                cv::imshow(comm_addr, mat);
                // std::cout << "show: " << count << ", size: " << data.size() << std::endl;
                auto key = cv::waitKey(1);
                if (key == 27) {
                    break;
                }
            } catch (std::exception &exception) {
                std::cout << exception.what() << std::endl;
                break;
            }

        }
    } else {
        std::cout << "Usage: <cvmat_pull_show> <addr_to_pull>\n" << std::endl;
    }
    return 0;
}