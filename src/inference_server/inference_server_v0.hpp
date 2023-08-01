//
// Created by cc on 2021/11/22.
//

#ifndef FFMPEG_WRAPPER_SRC__INFERENCE_SERVER__INFERENCE_SERVER_V0_HPP_
#define FFMPEG_WRAPPER_SRC__INFERENCE_SERVER__INFERENCE_SERVER_V0_HPP_

#include <string>
#include <memory>

namespace gddi {

/**
 * @brief a sample server for inference API
 *
 *        NOT ONLY Support HTTP, HTTPS, but also grpc, udp(private) protocol etc.
 *
 *        这是个实验性质的 推理 服务类
 */
class InferenceServer_v0 {
public:
    InferenceServer_v0();
    ~InferenceServer_v0();

    void launch(const std::string &host, uint32_t port);

    bool run_v0_task(const std::string &task_name, const std::string &json_text);

private:
    class InferenceServerImpl;
    std::unique_ptr<InferenceServerImpl> impl_;
};

} // END gddi

#endif //FFMPEG_WRAPPER_SRC__INFERENCE_SERVER__INFERENCE_SERVER_V0_HPP_
