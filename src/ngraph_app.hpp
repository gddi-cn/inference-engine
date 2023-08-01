//
// Created by cc on 2021/11/9.
//

#ifndef FFMPEG_WRAPPER_SRC_NGRAPH_APP_HPP_
#define FFMPEG_WRAPPER_SRC_NGRAPH_APP_HPP_

#include "utils.hpp"
#include "runnable_node.hpp"
#include "json.hpp"
#include "VariadicTable.h"
#include <stdexcept>

namespace gddi {
namespace ngraph {

class NodeRunner {
public:
    NodeRunner() = default;

    static void dump_help_info(std::ostream &oss);

    int parse_and_run(const std::string &config) {
        try {
            logs_.str("");
            auto my_json = nlohmann::json::parse(config);
            logs_ << "Config Version: " << my_json["version"].get<std::string>() << std::endl;
            if (is_v0(my_json)) {
                parse_nodes(my_json);
                std::cout << logs_.str();
                return run_local();
            }
            return -2;
        } catch (nlohmann::json::exception &e) {
            std::cerr << "message: " << e.what() << '\n'
                      << "exception id: " << e.id << std::endl;
        } catch (std::exception &e) {
            std::cerr << "message: " << e.what() << '\n';
        }
        return -1;
    }

private:
    static bool is_v0(const nlohmann::json &j) {
        return j.at("version").get<std::string>() == "v0";
    }

    void parse_nodes(const nlohmann::json &j) {
        if (j.contains("nodes")) {
            auto nodes = j.at("nodes");
            for (const auto &n : nodes) {
                create_node(n);
            }
        }
        if (j.contains("pipe")) {
            create_pipes(j.at("pipe"));
        }
    }
    void create_pipes(const nlohmann::json &pipes) {
        VariadicTable<std::string, std::string, std::string, std::string, std::string, std::string>
            variadic_table({"from", "ep out", "connect to", "to", "ep in", "Runner"});

        for (const auto &p: pipes) {
            auto from_ = nodes_.at(p.at(0).get<int>());
            auto ep_o_ = p.at(1).get<int>();
            auto to_ = nodes_.at(p.at(2).get<int>());
            auto ep_i_ = p.at(3).get<int>();

            auto success = from_->connect_to(to_, ep_o_, ep_i_);
            variadic_table.addRow(
                from_->name(),
                from_->get_endpoint_features(gddi::ngraph::endpoint::Type::Output, ep_o_).result
                    .data_features,
                "  =====>  ",
                to_->name(),
                to_->get_endpoint_features(gddi::ngraph::endpoint::Type::Input, ep_i_).result
                    .data_features,
                to_->get_runner()->name());

            std::stringstream oss;
            oss << from_->name() << "{"
                << from_->get_endpoint_features(gddi::ngraph::endpoint::Type::Output, ep_o_).result
                    .data_features << "} ====> "
                << to_->name() << "{"
                << to_->get_endpoint_features(gddi::ngraph::endpoint::Type::Input, ep_i_).result
                    .data_features
                << "}"
                << (success ? " success!" : " failure! endpoint data mismatch")
                << std::endl;

            if (!success) {
                throw std::runtime_error(oss.str().c_str());
            }
        }

        logs_ << "Connect Matrix:\n";
        variadic_table.print(logs_);
        logs_ << std::endl;
    }

    void create_node(const nlohmann::json &j) {
        auto id = j.at("id").get<int>();
        auto name = j.at("name").get<std::string>();
        auto type = j.at("type").get<std::string>();
        auto runner_id = j.at("runner").get<std::string>();

        if (nodes_.count(id) > 0) {
            std::stringstream oss;
            oss << "already has id: " << id << std::endl;
            throw std::runtime_error(oss.str().c_str());
        }

        auto runner = get_runner(runner_id);
        auto node = create_node(type, runner, name);
        if (node) {
            nodes_[id] = node;

            if (j.contains("props")) {
                auto props = j.at("props");
                for (auto &it: props.items()) {
                    if (it.value().is_number()) {
                        node->properties().try_set_property(it.key(), it.value().get<double>());
                    } else {
                        node->properties().try_set_property(it.key(), it.value().get<std::string>());
                    }
                }
            }

            logs_ << "Create Node: " << id << ", "
                  << name << ", " << utils::get_class_name(node.get())
                  << std::endl;
            node->print_info(logs_);
            logs_ << std::endl;
        } else {
            std::stringstream oss;
            oss << "fail to create node: " << type << std::endl;
            throw std::runtime_error(oss.str().c_str());
        }
    }

    std::shared_ptr<gddi::ngraph::Runner> &get_runner(const std::string &name) {
        if (runners_.count(name) == 0) {

            runners_[name] = std::make_shared<gddi::ngraph::Runner>(name);
            runners_[name]->set_quit_listener([this](int code) {
                for (const auto &r: runners_) {
                    r.second->post_quit_loop(code);
                }
                quit_code_ = code;
            });
        }
        return runners_[name];
    }

    int run_local() {
        for (const auto &r:runners_) {
            r.second->start();
        }
        for (const auto &r:runners_) {
            r.second->join();
        }
        return quit_code_;
    }

protected:
    static std::shared_ptr<gddi::ngraph::NodeAny> create_node(
        const std::string &type,
        const std::shared_ptr<ngraph::Runner> &runner,
        const std::string& node_name);

protected:
    std::stringstream logs_;
    int quit_code_{};
    std::unordered_map<int, std::shared_ptr<gddi::ngraph::NodeAny>> nodes_;
    std::unordered_map<std::string, std::shared_ptr<gddi::ngraph::Runner>> runners_;
};

}
}

#endif //FFMPEG_WRAPPER_SRC_NGRAPH_APP_HPP_
