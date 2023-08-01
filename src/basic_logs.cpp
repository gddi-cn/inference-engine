//
// Created by cc on 2022/2/8.
//
#include "basic_logs.hpp"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/base_sink.h"
#include "spdlog/details/null_mutex.h"
#include <mutex>
#include "modules/server_send/server_send.hpp"

extern "C" {
#include <libavutil/log.h>
}

#include <sstream>
#include "utils.hpp"

namespace gddi {
namespace logs {

std::shared_ptr<spdlog::logger> global_av_logger;

template<typename Mutex>
class ws_sink : public spdlog::sinks::base_sink<Mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg &msg) override {

        // log_msg is a struct containing the log entry info like level, timestamp, thread id etc.
        // msg.raw contains pre formatted log

        // If needed (very likely but not mandatory), the sink formats the message before sending it to its final destination:
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        //std::cout << fmt::to_string(formatted);
        gddi::server_send::instance().push_log(fmt::to_string(formatted));
    }

    void flush_() override {
        //std::cout << std::flush;
    }
};

using ws_sink_mt = ws_sink<std::mutex>;
using ws_sink_st = ws_sink<spdlog::details::null_mutex>;

void setup_spdlog(const spdlog::level::level_enum level_enum) {
    // https://github.com/gabime/spdlog/wiki/1.-QuickStart
    auto ws_server_sink = std::make_shared<ws_sink_mt>();
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    console_sink->set_level(level_enum);

    // console_sink->set_pattern("[multi_sink_example] [%^%l%$] %v");

    auto sinks_list = spdlog::sinks_init_list({console_sink, ws_server_sink});

    // auto lib_av_sink =  std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    global_av_logger = std::make_shared<spdlog::logger>("gddi-avc", sinks_list);

    auto default_logger = std::make_shared<spdlog::logger>("gddi", sinks_list);

    spdlog::set_default_logger(default_logger);
    spdlog::set_level(spdlog::level::debug);
}
}
}
