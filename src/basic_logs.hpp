//
// Created by cc on 2022/2/7.
//

#ifndef INFERENCE_ENGINE_SRC_BASIC_LOGS_HPP_
#define INFERENCE_ENGINE_SRC_BASIC_LOGS_HPP_

// #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h> // must be included
#include <spdlog/sinks/stdout_sinks.h>

namespace gddi {
namespace logs {
void setup_spdlog(const spdlog::level::level_enum level_enum);
}

}

#endif //INFERENCE_ENGINE_SRC_BASIC_LOGS_HPP_
