//
// Created by cc on 2021/11/1.
//
#include <utility>
#include "ngraph_app.hpp"

int main(int argc, char *argv[]) {
    gddi::ngraph::NodeRunner node_runner;
    std::string config;
    if (gddi::utils::read_file("./../../../../data/node_sample.json", config)) {
        return node_runner.parse_and_run(config);
    }
    return 0;
}