//
// Created by cc on 2021/11/29.
//

#ifndef FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_TASK_JSON_HPP_
#define FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_TASK_JSON_HPP_
#include <string>
#include <sstream>
#include <map>
#include <set>
#include "./_inference_types.hpp"

class InferenceSliceConfig {
public:
    struct NodeConfig {
        int id{};
        std::string name;
        std::string type;
        std::string runner;
        std::map<std::string, nlohmann::json> props;
    };

    struct NodePipeConfig {
        union {
            int conn[4];
            struct {
                int from;
                int from_ep;
                int to;
                int to_ep;
            };
        };

    };

    static ResultMsg parse(const std::string &text, InferenceSliceConfig &config) {
        std::stringstream logs("");
        bool success = false;
        try {
            auto json = nlohmann::json::parse(text);
            config.get_version(json);
            config.get_nodes(json);
            config.get_pipes(json);
            config.raw_json = text;
            success = true;
        } catch (nlohmann::json::exception &e) {
            logs << "message: " << e.what() << '\n'
                 << "exception id: " << e.id << std::endl;
        } catch (std::exception &e) {
            logs << "message: " << e.what();
        }
        return {success, logs.str()};
    }

    std::map<int, NodeConfig> node_configs;             // id, NodeConfig
    std::map<std::string, NodePipeConfig> node_flows;   // key(generated), NodePipeConfig
    std::set<std::string> named_runners;
    std::string version;
    std::string raw_json;

private:
    void get_version(nlohmann::json &json) { version = json["version"].get<std::string>(); }
    void get_nodes(nlohmann::json &json) {
        auto nodes = json["nodes"];
        for (const auto &node : nodes) {
            NodeConfig config;
            config.id = node["id"].get<int>();
            config.type = node["type"].get<std::string>();
            config.name = node["name"].get<std::string>();
            config.runner = node["runner"].get<std::string>();

            if (node.contains("props")) {
                auto props = node["props"];
                for (auto &iter : props.items()) {
                    config.props[iter.key()] = iter.value();
                }
            }
            auto iter = node_configs.find(config.id);
            if (iter != node_configs.end()) {
                std::stringstream errs;
                errs << "id: < " << config.id << " > already defined!!!";
                throw config_parse_error(errs.str().c_str());
            }
            node_configs[config.id] = config;
            named_runners.insert(config.runner);
        }
    }

    void ensure_defined_node(int node_id) const {
        if (node_configs.count(node_id) == 0) {
            std::stringstream errs;
            errs << "node: < " << node_id << " > not defined!!!";
            throw config_parse_error(errs.str().c_str());
        }
    }
    void get_pipes(nlohmann::json &json) {
        auto pipes = json["pipe"];
        for (const auto &pipe : pipes) {
            if (pipe.is_array()) {
                int from_ = pipe.at(0).get<int>();
                int from_ep_ = pipe.at(1).get<int>();
                int to_ = pipe.at(2).get<int>();
                int to_ep_ = pipe.at(3).get<int>();
                std::stringstream named_key;
                named_key << "["
                          << from_ << ", "
                          << from_ep_ << ", "
                          << to_ << ", "
                          << to_ep_ << "]";

                auto the_key = named_key.str();
                if (node_flows.count(the_key) > 0) {
                    std::stringstream errs;
                    errs << "pipe: < " << the_key << " > already defined!!!";
                    throw config_parse_error(errs.str().c_str());
                }
                ensure_defined_node(from_);
                ensure_defined_node(to_);

                auto &cfg = node_flows[the_key];
                cfg.from = from_;
                cfg.from_ep = from_ep_;
                cfg.to = to_;
                cfg.to_ep = to_ep_;
            }
        }
    }

};

#endif //FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_TASK_JSON_HPP_
