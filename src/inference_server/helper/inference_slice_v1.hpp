//
// Created by cc on 2022/1/29.
//

#ifndef INFERENCE_ENGINE_SRC__INFERENCE_SERVER__HELPER_INFERENCE_SLICE_V1_HPP_
#define INFERENCE_ENGINE_SRC__INFERENCE_SERVER__HELPER_INFERENCE_SLICE_V1_HPP_
#include "inference_helper.hpp"
#include "runnable_node.hpp"
#include "utils.hpp"
#include <atomic>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <modules/server_send/ws_notifier.hpp>
#include <sstream>
#include <string>
#include <set>

static int stream_id{0};

class inference_slice : public std::enable_shared_from_this<inference_slice> {
private:
    struct running_ctrl_ctx {
        // 当前slice的别名，任务名etc.
        std::string name;

        // 当前slice对应的原始配置
        std::string raw_json;

        // 当前slice总共使用的runner
        uint32_t total_runners{0};

        // runner thread 退出代码
        std::atomic_int32_t quit_code{0};

        // runner thread 启动后，退出的runner计数，用于判断是否全部都退出来了。
        std::atomic_uint32_t quit_runner_num{0};

        // 当所有的runner都退出后，会调用此函数
        std::function<void(int)> on_quit;

        // 标记slice退出状态
        bool slice_stopped{false};

        void runner_quit_with(int code, const std::string &runner_name) {
            quit_code = code;
            quit_runner_num++;

            slice_stopped = true;
            if (quit_runner_num.load() == total_runners) {
                if (on_quit) { on_quit(code); }
            }
        }
    };

private:
    // 用于hold变量，使用shared是为了在当前实例释放后，异步回调里也可以正常的访问。
    std::shared_ptr<running_ctrl_ctx> running_ctrl_ctx_;

    // 当前配置引用到的runner
    std::map<std::string, std::shared_ptr<gddi::ngraph::Runner>> named_runners_;

    // 当前配置创建的所有节点
    std::map<int, std::shared_ptr<gddi::ngraph::NodeAny>> nodes_;

    // 用于标识启动，每个inference_slice在设计上只允许正向跑一次。多次是不允许的。
    int32_t run_times_{0};

private:
    inference_slice() = default;

    /**
     * @brief 获取 runner_id 对应的 runner 对象，如果不存在，则动态创建。
     * @param runner_id runner 对象的名称，需要在整个 slice 里是保持唯一的。
     * @return
     */
    std::shared_ptr<gddi::ngraph::Runner> get_runner_(const std::string &runner_id) {
        auto iter = named_runners_.find(runner_id);
        if (iter == named_runners_.end()) {
            named_runners_[runner_id] = std::make_shared<gddi::ngraph::Runner>(runner_id);
        }
        return named_runners_[runner_id];
    }

public:
    ~inference_slice() = default;

    /**
     * @brief 执行(启动)整个 slice 包含的 runner 对象线程。
     *
     *        注意，整个 slice 对象，只可以运行一次，如果要多次运行，需要删除重新创建。
     * @param on_slice_quit 整个 slice 退出的时候的
     */
    void run_slice(const std::function<void(int)> &on_slice_quit) {
        // Only run once
        if (run_times_ > 0) { return; }
        run_times_++;
        running_ctrl_ctx_->on_quit = on_slice_quit;

        // 1. Setup all runner quit function
        for (const auto &runner : named_runners_) {
            auto local_ctx = running_ctrl_ctx_;
            auto local_rid = runner.first;
            runner.second->set_quit_listener(
                [=](int quit_code) { local_ctx->runner_quit_with(quit_code, local_rid); });
        }

        // 2. Start all runner thread
        for (const auto &r : named_runners_) { r.second->start(); }
    }

    bool is_running() const {
        if (run_times_ > 0) { return !running_ctrl_ctx_->slice_stopped; }
        return false;
    }

    const std::map<std::string, std::shared_ptr<gddi::ngraph::Runner>> &runners() const {
        return named_runners_;
    }
    const std::map<int, std::shared_ptr<gddi::ngraph::NodeAny>> &nodes() const { return nodes_; }
    const std::string &raw_json() const { return running_ctrl_ctx_->raw_json; }
    int32_t quit_code() const { return running_ctrl_ctx_->quit_code.load(); }

    std::shared_ptr<gddi::ngraph::NodeAny> first_node() const { return nodes_.begin()->second; }
    std::shared_ptr<gddi::ngraph::NodeAny>
    find_first_node_by_type(const std::string &n_type) const {
        for (const auto &item : nodes_) {
            if (item.second->type() == n_type) { return item.second; }
        }
        return nullptr;
    }
    std::shared_ptr<gddi::ngraph::NodeAny>
    find_first_node_by_name(const std::string &n_name) const {
        for (const auto &item : nodes_) {
            if (item.second->name() == n_name) { return item.second; }
        }
        return nullptr;
    }
    std::shared_ptr<gddi::ngraph::NodeAny>
    find_last_node_by_type(const std::string &n_type) const {
        for (auto iter = nodes_.crbegin(); iter != nodes_.crend(); iter++) {
            if (iter->second->type() == n_type) { return iter->second; }
        }
        return nullptr;
    }
    std::shared_ptr<gddi::ngraph::NodeAny>
    find_last_node_of_preview() const {
        for (auto iter = nodes_.crbegin(); iter != nodes_.crend(); iter++) {
            auto &feature_flags = iter->second->properties().feature_flags();
            if (feature_flags.count("support_preview") > 0 && feature_flags["support_preview"].get<bool>()) {
                return iter->second;
            }
        }
        return nullptr;
    }
    std::shared_ptr<gddi::ngraph::NodeAny> find_node_by_id(int node_id) const {
        auto iter = nodes_.find(node_id);
        if (iter != nodes_.end()) { return iter->second; }
        return nullptr;
    }

    // 兼容旧代码函数，未来可能删除
    void release_runners_nodes() {
        nodes_.clear();
        named_runners_.clear();
    }

public:
    static std::shared_ptr<inference_slice> create_from(const std::string &slice_name,
                                                        const InferenceSliceConfig &task_json) {
        auto slice_ = std::shared_ptr<inference_slice>(new inference_slice);

        // 1. create all runners
        for (const auto &name : task_json.named_runners) { slice_->get_runner_(name); }

        // 2. create all node with runners
        auto &task_all_nodes = slice_->nodes_;// ref the var member
        for (const auto &n : task_json.node_configs) {
            const auto &cfg = n.second;
            auto runner = slice_->get_runner_(cfg.runner);
            auto p_node = gddi::NodeManager::get_instance().create(cfg.type, runner, cfg.name);

            if (p_node == nullptr) {
                // fail to create such typed node<cfg.type>
                // 正常来说，是不会执行到这里的。如果配置是正确的话
                throw std::runtime_error(std::string("cannot create node for: ") + cfg.type);
            }
            if (task_all_nodes.count(cfg.id) > 0) {// id 已经存在了
                throw std::runtime_error(std::string("node id already exist. ") + cfg.name);
            }

            // 2.1 setup props
            for (auto &prop : cfg.props) {
                p_node->properties().try_set_property(prop.first, prop.second);
            }
            // 内部参数
            p_node->properties().try_set_property("task_name", slice_name);
            p_node->properties().try_set_property("stream_id", stream_id++);// 后续会移除

            task_all_nodes[cfg.id] = p_node;
        }

        // 3. Link all pipes
        for (const auto &line : task_json.node_flows) {
            const auto &cfg = line.second;
            auto &from_ = task_all_nodes[cfg.from];
            auto &to_ = task_all_nodes[cfg.to];

            if (!from_->connect_to(to_, cfg.from_ep, cfg.to_ep)) {
                // fail to connect with target node
                std::stringstream oss;
                using gddi::ngraph::endpoint::Type;
                oss << from_->name() << "{"
                    << from_->get_endpoint_features(Type::Output, cfg.from_ep).result.data_features
                    << "} ====> " << to_->name() << "{"
                    << to_->get_endpoint_features(Type::Input, cfg.to_ep).result.data_features
                    << "}"
                    << " failure! endpoint data mismatch" << std::endl;
                std::cerr << oss.str() << std::endl;
                throw std::runtime_error(oss.str());
            }

            to_->input_endpoint_increment();
        }

        slice_->running_ctrl_ctx_ = std::make_shared<running_ctrl_ctx>();
        slice_->running_ctrl_ctx_->raw_json = task_json.raw_json;
        slice_->running_ctrl_ctx_->name = slice_name;
        slice_->running_ctrl_ctx_->total_runners = slice_->named_runners_.size();
        return slice_;
    }

    static std::shared_ptr<inference_slice> create_slice_from(const std::string &slice_template,
                                                              const nlohmann::json &args) {
        auto &&slice_text = load_template(slice_template, args);
        InferenceSliceConfig config;
        auto result = InferenceSliceConfig::parse(slice_text, config);
        if (result.success) { return inference_slice::create_from(slice_template, config); }
        throw std::runtime_error(result.result);
    }
};

inline std::shared_ptr<inference_slice>
attach_slice_for(const std::shared_ptr<gddi::ngraph::NodeAny> &node_to_attach,
                 const std::string &slice_path, const nlohmann::json &args) {
    auto slice_ = inference_slice::create_slice_from(slice_path, args);
    node_to_attach->connect_to(slice_->first_node());
    return slice_;
}

inline std::string
dump_runtime_json(const std::vector<std::shared_ptr<gddi::ngraph::NodeAny>> &nodes_) {
    auto node_vec = nlohmann::json::array();
    auto pipe_vec = nlohmann::json::array();
    auto runtime_json = nlohmann::json::object();
    std::vector<gddi::NodeConnectionLine> node_connections;
    std::map<const void *, nlohmann::json> node_map;
    std::map<const void *, int> node_new_id;
    int id_base = 0;

    for (const auto &n : nodes_) {
        nlohmann::json json_node;

        json_node["id"] = id_base;
        json_node["type"] = n->type();
        json_node["name"] = n->name();
        json_node["runner"] = n->get_runner()->name();

        const auto &items = n->properties().items();
        nlohmann::json props;
        for (const auto &item : items) { props[item.first] = item.second.get_runtime_value(); }
        json_node["props"] = props;

        n->get_connections(node_connections);
        node_map[n.get()] = json_node;
        node_new_id[n.get()] = id_base;

        node_vec.push_back(json_node);
        id_base++;
    }

    for (const auto &c : node_connections) {
        std::vector<int> pipe_ent;
        pipe_ent.push_back(node_new_id[c.from_]);
        pipe_ent.push_back(c.from_ep_);
        pipe_ent.push_back(node_new_id[c.to_]);
        pipe_ent.push_back(c.to_ep_);
        pipe_vec.push_back(pipe_ent);
    }

    runtime_json["version"] = "v0";
    runtime_json["nodes"] = node_vec;
    runtime_json["pipe"] = pipe_vec;

    return runtime_json.dump();
}

#endif//INFERENCE_ENGINE_SRC__INFERENCE_SERVER__HELPER_INFERENCE_SLICE_V1_HPP_
