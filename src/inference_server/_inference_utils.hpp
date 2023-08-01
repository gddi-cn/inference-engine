//
// Created by cc on 2021/11/29.
//

#ifndef FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_UTILS_HPP_
#define FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_UTILS_HPP_
#include <sstream>

class OStreamFlagsHolder {
public:
    explicit OStreamFlagsHolder(std::ostream &oss) : oss_(oss), flags_(oss.flags()) {}
    ~OStreamFlagsHolder() { oss_.flags(flags_); }
private:
    std::ostream &oss_;
    std::ostream::fmtflags flags_;
};

#endif //FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_UTILS_HPP_
