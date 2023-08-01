//
// Created by cc on 2021/11/23.
//
#include "basic_logs.hpp"
#include "inference_server/inference_server_v0.hpp"
#include "version.h"
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <string>

using namespace boost::program_options;

int main(int argc, char *argv[]) {
    try {
        gddi::InferenceServer_v0 inference_server;

        options_description desc("all options");
        variables_map vm;

        desc.add_options()("help,h", "show help")("version,v", "show version")(
            "svr-addr", value<std::string>()->default_value("0.0.0.0"), "http server address")(
            "svr-port", value<uint32_t>()->default_value(8780), "http server port")(
            "cfg-file", value<std::string>()->default_value(""), "local config file")(
            "log-level", value<int>()->default_value(2), "0: trace; 1: debug; 2: info");

        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        } else if (vm.count("version")) {
            std::cout << PROJECT_VERSION << " " << GIT_BRANCH << "-" << GIT_HASH << " "
                      << BUILD_TIME << std::endl;
            return 0;
        }

        gddi::logs::setup_spdlog(
            static_cast<spdlog::level::level_enum>(vm["log-level"].as<int>()));

        auto cfg_file = vm["cfg-file"].as<std::string>();
        if (cfg_file.size() > 0) {
            std::fstream file(cfg_file);
            std::string cfg_content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
            if (inference_server.run_v0_task("direct_task", cfg_content)) {
                inference_server.launch(vm["svr-addr"].as<std::string>(),
                                        vm["svr-port"].as<uint32_t>());
            }
        } else {
            inference_server.launch(vm["svr-addr"].as<std::string>(),
                                    vm["svr-port"].as<uint32_t>());
        }

    } catch (const spdlog::spdlog_ex &ex) {
        std::cout << "Log initialization failed: " << ex.what() << std::endl;
    }

    return 0;
}