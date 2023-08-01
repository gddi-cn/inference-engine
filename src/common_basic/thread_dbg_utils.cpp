//
// Created by cc on 2022/6/23.
//
#include "thread_dbg_utils.hpp"
#include <boost/algorithm/string/predicate.hpp>
#if __linux__
#include <pthread.h>
#endif

namespace gddi::thread_utils {
std::string get_cur_thread_name() {
    std::string cur_name;
#if __linux__
    char name_buf[256];
    pthread_getname_np(pthread_self(), name_buf, sizeof(name_buf) - 1);
    cur_name = name_buf;
#endif
    return cur_name;
}

std::string eval_thread_name(const std::string &name) {
    if (boost::algorithm::starts_with(name, "r|")) { return name; }
    if (boost::algorithm::starts_with(name, "r:")) { return "r|" + name.substr(2); }
    return "r|" + name;
}

void set_cur_thread_name(const std::string &name) {
#if __linux__
    pthread_setname_np(pthread_self(), eval_thread_name(name).substr(0, 15).c_str());
#endif
}
}// namespace gddi::thread_utils
