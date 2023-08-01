//
// Created by cc on 2021/11/29.
//

#ifndef FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_TYPES_HPP_
#define FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_TYPES_HPP_
#include <stdexcept>
#include "runnable_node.hpp"

typedef gddi::result_for<std::string> ResultMsg;

class config_parse_error : public std::exception {
public:
    explicit config_parse_error(const char *error) : error_(error) {}
    const char *what() const noexcept override { return error_; }
    const char *const error_;
};

#endif //FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_TYPES_HPP_
