//
// Created by cc on 2021/11/1.
//
#include <utility>
#include "ngraph_app.hpp"

int main(int argc, char *argv[]) {
    gddi::ngraph::NodeRunner node_runner;
    std::string config;
    if (argc > 1) {
        if (gddi::utils::read_file(argv[1], config)) {
            return node_runner.parse_and_run(config);
        }
    } else {
        std::cout << "Usage: <app> path_to_config_json\n" << std::endl;
    }
    return 0;
}