//
// Created by cc on 2021/11/18.
//

#ifndef FFMPEG_WRAPPER_SRC_NODES_NODE_MANAGER_HPP_
#define FFMPEG_WRAPPER_SRC_NODES_NODE_MANAGER_HPP_
#include <mutex>
#include <memory>
#include <functional>
#include <unordered_map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <utility>
#include <VariadicTable.h>
#include <json.hpp>
#include "runnable_node.hpp"
#include "utils.hpp"
#include <boost/type_index.hpp>
#include "basic_managed_shared.hpp"

namespace gddi {

class NodeManager {
private:
    NodeManager() = default;

public:
    NodeManager(const NodeManager &) = delete;
    NodeManager(NodeManager &&) = delete;
    NodeManager &operator=(const NodeManager &) = delete;
    NodeManager &operator=(NodeManager &&) = delete;

    static NodeManager &get_instance() {
        static std::mutex mutex;
        static std::shared_ptr<NodeManager> node_manager = nullptr;
        std::lock_guard<std::mutex> lock_guard(mutex);
        if (node_manager == nullptr) {
            // utils::windows_utf8_cout();
            node_manager = std::shared_ptr<NodeManager>(new NodeManager());
            node_manager->bind_all_node_to_node_manager();
        }
        return *node_manager;
    }

    template<class T>
    std::string template_type_name() {
        auto typed_name = boost::typeindex::type_id<T>().pretty_name();
        auto pos = typed_name.rfind(':');
        if (pos == std::string::npos) {
            return typed_name;
        }
        return typed_name.substr(pos + 1);
    }

    template<class TyNodeClass_>
    void bind_node_creator(const std::string &description) {
        auto type_name = template_type_name<TyNodeClass_>();
        if (node_creators_.find(type_name) == node_creators_.end()) {
            auto &entry_ = node_creators_[type_name];
            entry_.name = node_name_(type_name);
            entry_.version = guess_version_(type_name);
            entry_.description = description;
            entry_.node_creator = [this, type_name](const std::shared_ptr<ngraph::Runner> &r_, const std::string &n_) {
                // auto node_ = std::make_shared<TyNodeClass_>(n_);
                auto node_ = get_managed_shared().create_object<TyNodeClass_>(n_);
                node_->set_runtime_type(type_name);
                node_->bind_runner(r_);
                return node_;
            };
        }
    }

    nlohmann::json get_node_constraints();
    nlohmann::json get_node_constraints_v2();

    std::ostream &print_std_term_style(std::ostream &oss);
    std::vector<std::string> get_names() const;

    struct NodeInfo {
        std::string name;
        std::string version;
        std::string description;
    };
    std::vector<NodeInfo> get_infos() const;

    std::shared_ptr<ngraph::NodeAny> create(
        const std::string &type,
        const std::shared_ptr<ngraph::Runner> &runner,
        std::string node_name);

private:
    void bind_all_node_to_node_manager();

    static std::string node_name_(const std::string &key_str);
    static std::string guess_version_(const std::string &key_str);

private:
    using NodeCreator = std::function<
        std::shared_ptr<ngraph::NodeAny>
            (const std::shared_ptr<ngraph::Runner> &, std::string)>;

    struct NodeCreatorEntry {
        std::string name;
        std::string version;
        std::string description;
        NodeCreator node_creator;
    };

    std::unordered_map<std::string, NodeCreatorEntry> node_creators_;
    static yacd::experimental::memory::basic_managed_shared &get_managed_shared() {
        static yacd::experimental::memory::basic_managed_shared basic_managed_shared_;
        return basic_managed_shared_;
    };
};

} // END gddi

#endif //FFMPEG_WRAPPER_SRC_NODES_NODE_MANAGER_HPP_
