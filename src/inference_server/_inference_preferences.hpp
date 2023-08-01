//
// Created by cc on 2021/11/29.
//

#ifndef FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_PREFERENCES_HPP_
#define FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_PREFERENCES_HPP_
#include <string>

/**
 * @brief 默认初始化配置
 */
class InferencePreferences {
public:
    InferencePreferences() = default;

    std::string debug_www_dir{"./data/debug_api_www"};
    std::string http_service_api_prefix{"/api"};
    std::string auth_service_url;
    std::string host{"0.0.0.0"};
    int http_port{8780};
    int ws_port{8781};  // ws: 8781, wss: 8782
};

#endif //FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_PREFERENCES_HPP_
