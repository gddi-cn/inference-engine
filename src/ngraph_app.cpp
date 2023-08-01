//
// Created by cc on 2021/11/9.
//

#include "node_manager/node_manager.hpp"
#include "ngraph_app.hpp"

// using namespace gddi::nodes;

namespace gddi {
namespace ngraph {

template<class TyNodes>
void bind_node_creator(const std::string &brief = std::string()) {
    gddi::NodeManager::get_instance().bind_node_creator<TyNodes>(brief);
}

void NodeRunner::dump_help_info(std::ostream &oss) {
    auto runner = std::make_shared<gddi::ngraph::Runner>("default");
    auto infos_ = gddi::NodeManager::get_instance().get_infos();

    oss << "# AvailableTypes\n\n";
    for (const auto &info: infos_) {
        oss << "* [`" << info.name << "`](##" << info.name << ")" << std::endl;
    }
    oss << std::endl;

    std::vector<std::shared_ptr<gddi::ngraph::NodeAny>> tmp_nodes;
    oss << "# Type Detail\n\n";

    for (const auto &info: infos_) {
        oss << "## `" << info.name << "`\n\n";

        if (!info.description.empty()) {
            oss << info.description << std::endl << std::endl;
        }

        auto n = gddi::NodeManager::get_instance().create(info.name, runner, info.name);
        n->print_info(oss);
        tmp_nodes.push_back(n);
        oss << std::endl;
    }

    oss << std::endl << std::endl;
}

std::shared_ptr<gddi::ngraph::NodeAny> NodeRunner::create_node(const std::string &type,
                                                               const std::shared_ptr<ngraph::Runner> &runner,
                                                               const std::string& node_name) {
    return gddi::NodeManager::get_instance().create(type, runner, node_name);
}
}
}