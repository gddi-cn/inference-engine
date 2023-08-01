//
// Created by cc on 2022/6/23.
//

#ifndef INFERENCE_ENGINE_SRC_COMMON_BASIC_THREAD_DBG_UTILS_HPP_
#define INFERENCE_ENGINE_SRC_COMMON_BASIC_THREAD_DBG_UTILS_HPP_

#include <string>
#include <thread>

namespace gddi {
namespace thread_utils {
std::string get_cur_thread_name();
void set_cur_thread_name(const std::string &name);
}
}

#endif //INFERENCE_ENGINE_SRC_COMMON_BASIC_THREAD_DBG_UTILS_HPP_
