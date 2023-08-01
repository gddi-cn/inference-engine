//
// Created by cc on 2021/11/29.
//

#ifndef FFMPEG_WRAPPER_SRC__INFERENCE_SERVER__INFERENCE_SERVER_BASIC_HPP_
#define FFMPEG_WRAPPER_SRC__INFERENCE_SERVER__INFERENCE_SERVER_BASIC_HPP_
#include <string>
#include <json.hpp>
#include "./_inference_types.hpp"

/**
 * @brief 核心服务 API 抽象规范
 */
class InferenceServerBasic {
public:
    InferenceServerBasic() = default;
    virtual ~InferenceServerBasic() = default;

    /**
     * @brief create v0 version task, from json config
     *
     *        thread-safe
     *
     * @param task_name
     * @param text json text file
     * @return
     */
    virtual ResultMsg create_task(const std::string &task_name, const std::string &text) = 0;

    /**
     * @brief Delete the task, remove task struct info from Class-Private-Data
     *
     *        thread-safe
     *
     * @param task_name
     * @return
     */
    virtual ResultMsg delete_task(const std::string &task_name) = 0;

    /**
     * @brief get all task info, return in JSON object
     * @return
     */
    virtual nlohmann::json get_tasks_json() = 0;

    /**
     * @brief preview
     * 
     * @param codec         software or hardware
     * @param stream_url    push stream url
     * @return ResultMsg
     */
    virtual ResultMsg preview_task(const std::string &task_name,
                                   const std::string &codec,
                                   const std::string &stream_url,
                                   int node_id) = 0;

    virtual ResultMsg task_preview_jpeg(const std::string &task_name, const std::string &node_name, const int scale) = 0;
    virtual ResultMsg task_preview_jpeg_close(const std::string &task_name, const std::string &node_name) = 0;
};

#endif //FFMPEG_WRAPPER_SRC__INFERENCE_SERVER__INFERENCE_SERVER_BASIC_HPP_
