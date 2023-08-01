//
// Created by cc on 2021/11/1.
//

#ifndef FFMPEG_WRAPPER_SRC_RUNNABLE_NODE_HPP_
#define FFMPEG_WRAPPER_SRC_RUNNABLE_NODE_HPP_
#include <memory>
#include <string>
#include <concurrentqueue.h>
#include <blockingconcurrentqueue.h>
#include <mutex>
#include <unordered_map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <functional>
#include <cstdlib>
#include <cstdio>
#include <utility>
#include <string>
#include <type_traits>
#include <chrono>
#include <list>
#include <map>
#include "utils.hpp"
#include "debug_tools.hpp"
#include "node_property_table.hpp"
#include "basic_logs.hpp"

namespace gddi {

struct NodeConnectionLine {
    const void *from_;
    const void *to_;
    int from_ep_;
    int to_ep_;
};

enum class TaskErrorCode {
    kNormal = 0,
    kDemuxer = 101,
    kDecoder,
    kAvToCv,
    kInference,
    kEncoder,
    kRemuxer,
    kRoiFilter,
    kImageServer,
    kBmHandle,
    kLogicGate,
    kRoiCrop,
    kCrossCounter,
    kSegCalculation,
    kFeatureLibrary,
    kPerspectiveTransform,
    kInvalidArgument,

    kZmq = 1001,
};

template<class ResultType>
struct result_for {
    bool success;
    ResultType result;
};

namespace ngraph {
class NodeAny; // pre-define
class Message {
public:
    Message() {
        timestamp.created = std::chrono::high_resolution_clock::now();
    };
    virtual ~Message() = default;
    Message(const Message &) = delete;              // No Copy
    Message(Message &&) = delete;                   // No Move
    Message &operator=(const Message &) = delete;   // No Copy by reload operator =

    virtual std::string to_string() const = 0;
    virtual std::string name() const = 0;

    struct ReferenceInfo {
        int64_t id;
        std::chrono::high_resolution_clock::time_point created;
    };
    struct Timestamp {
        ReferenceInfo ref;
        std::chrono::high_resolution_clock::time_point created;
    };
    Timestamp timestamp{0};

    void copy_timestamp_ref_info(const std::shared_ptr<Message> &r) {
        timestamp.ref = r->timestamp.ref;
    }

    // Runner init
    size_t message_queued_size;
};

typedef std::shared_ptr<Message> MessagePtr;

namespace endpoint {
enum class Type {
    Input,
    Output
};

struct DataFeatures {
    std::string data_features;
};

bool is_connectable(const DataFeatures &from, const DataFeatures &to);
bool is_valid(const DataFeatures &data_features);
}

/**
 * @brief 运行节点线程，保证线程安全，所提供的每一个公开接口调用
 * 
 */
class Runner : public std::enable_shared_from_this<Runner> {
public:
    Runner() = delete;
    explicit Runner(std::string name)
        : name_(std::move(name)),
          attached_node_manager_(std::make_unique<AttachedNodeManager>(this)) {}
    virtual ~Runner() {
        stop();
        spdlog::debug("destructor: {}, Runner: {}", utils::get_class_name(this), name_);
    }

    const std::string &name() const { return name_; }

    void start() {
        std::lock_guard<std::mutex> lock_guard(mutex_);

        _stop();
        thread_ = std::thread([this]() { _run(); });
    }

    void start_in_local() { _run(); }

    void stop() {
        std::lock_guard<std::mutex> lock_guard(mutex_);

        _stop();
    }

    /**
     * @brief
     * @param on_quit 这个函数会在整个线程退出后执行，不会在线程退出的当前环境执行。
     *
     *                所以，不要绑定任何这个线程相关的资源。
     *                不然，一定BUG。
     */
    void set_quit_listener(std::function<void(int)> on_quit) {
        std::lock_guard<std::mutex> lock_guard(mutex_);

        on_quit_ = std::move(on_quit);
    }

    void join() {
        std::lock_guard<std::mutex> lock_guard(mutex_);
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void post_quit_loop(int code) {
        message_queue_.enqueue({std::weak_ptr<NodeAny>(), code, nullptr});
    }

    using MessageDispatch = std::function<void(const std::weak_ptr<NodeAny> &node,
                                               int endpoint,
                                               const MessagePtr &message)>;

    MessageDispatch attach_node(NodeAny *node_any, std::function<void()> on_setup) {
        auto dispatch = attached_node_manager_->attach_node(node_any, std::move(on_setup));
        _wakeup_queue_once();
        return dispatch;
    }

protected:
    void _run();

    void _stop() {
        if (thread_.joinable()) {
            message_queue_.enqueue({std::weak_ptr<NodeAny>(), 0, nullptr});
            thread_.join();
        }
    }

    /**
     * @brief thread-safe push_input_endpoint
     * @param node
     * @param endpoint
     * @param message
     */
    void dispatch_message(const std::weak_ptr<NodeAny> &node,
                          int endpoint,
                          const MessagePtr &message);

private:
    struct AttachedNode {
        NodeAny *node{};
        std::function<void()> on_setup;
    };

    class AttachedNodeManager {
    public:
        explicit AttachedNodeManager(Runner *runner) : runner_(runner) {}
        ~AttachedNodeManager() = default;

        MessageDispatch attach_node(NodeAny *node_any, std::function<void()> on_setup) {
            auto weak = push_for_init(node_any, std::move(on_setup));

            return [weak](const std::weak_ptr<NodeAny> &node, int endpoint, const MessagePtr &message) {
                auto runner = weak.lock();
                if (runner) { runner->dispatch_message(node, endpoint, message); }
            };
        }

        std::unordered_map<NodeAny *, AttachedNode> take_attach_nodes() {
            std::lock_guard<std::mutex> lock_guard(mutex_);

            auto moved = std::move(attached_nodes_);
            return moved;
        }

    protected:
        std::weak_ptr<Runner> push_for_init(NodeAny *node_any, std::function<void()> on_setup) {
            std::lock_guard<std::mutex> lock_guard(mutex_);

            AttachedNode attached_node;
            attached_node.node = node_any;
            attached_node.on_setup = std::move(on_setup);
            attached_nodes_[node_any] = attached_node;

            return runner_->weak_from_this();
        }

    private:
        std::mutex mutex_;
        std::unordered_map<NodeAny *, AttachedNode> attached_nodes_;
        Runner *runner_;
    };

    void _wakeup_queue_once() {
        message_queue_.enqueue({std::weak_ptr<NodeAny>(), 0, nullptr, true});
    }

private:
    struct RunnerMessage {
        std::weak_ptr<NodeAny> receiver;
        int port_in{};
        MessagePtr message;
        bool is_echo{false};
    };

    moodycamel::BlockingConcurrentQueue<RunnerMessage> message_queue_;
    std::mutex mutex_;
    std::thread thread_;
    std::string name_;
    std::function<void(int)> on_quit_;
    std::unique_ptr<AttachedNodeManager> attached_node_manager_;
};

/**
 * @brief thread-safe, all
 */
class NodeAny : public std::enable_shared_from_this<NodeAny> {
private:
    class NodeOutputManager;

public:
    NodeAny() = delete;
    explicit NodeAny(std::string name = std::string());
    virtual ~NodeAny();

    virtual void print_info(std::ostream &oss) const {}

    const std::string &name() const { return name_; }
    const std::string &type() const { return type_; }
    void set_runtime_type(const std::string &r_type) { type_ = r_type; }
    std::shared_ptr<Runner> get_runner() const { return node_runner_.lock(); }
    void bind_runner(const std::shared_ptr<Runner> &runner);

    result_for<endpoint::DataFeatures> get_endpoint_features(
        endpoint::Type type, int endpoint) const {
        std::lock_guard<std::mutex> lock_guard(mutex_); // 访问保护
        return on_query_endpoint(type, endpoint);
    }

    inline
    void push_input_endpoint(int endpoint, const MessagePtr &message) {
        dispatch_(shared_from_this(), endpoint, message);
    }

    virtual std::pair<int, std::map<int, std::string>> get_logs() const {
        return node_logs_;
    }

    /**
     * @brief 如果正在<connect_to>，这里会被阻塞。
     * @param endpoint
     * @param message
     */
    void push_output_endpoint(int endpoint, const MessagePtr &message);

    /**
     * @brief 理论上，现在可以在设置完成后，任何时候调用此函数完成不同节点间的数据交互需求
     *        两个节点连接时，受shared_ptr，目标都是存在的。并且endpoint的属性不会发生变化!!!
     * @param target_node
     * @param output_endpoint
     * @param target_endpoint
     * @return true for connect success
     */
    bool connect_to(
        const std::shared_ptr<NodeAny> &target_node,
        int output_endpoint = 0,
        int target_endpoint = 0);

    void get_connections(std::vector<NodeConnectionLine> &connections) const;

public:
    NodePropertyTable &properties() { return property_table_; }
    const NodePropertyTable &properties() const { return property_table_; }

    template<class Ty_>
    void bind_simple_flags(const std::string &key, const Ty_ &value) {
        property_table_.bind_simple_flags(key, value);
    }

    template<class Ty_>
    void bind_simple_property(const std::string &key, Ty_ &value, const std::vector<Ty_> &options,
                              const std::string &desc = {}, const PropAccess access = PropAccess::kPublic) {
        property_table_.bind_simple_property(key, value, options, desc, access);
    }

    template<class Ty_>
    void bind_simple_property(const std::string &key, Ty_ &value, const std::string &desc = {},
                              const PropAccess access = PropAccess::kPublic) {
        property_table_.bind_simple_property(key, value, {}, desc, access);
    }

    template<class Ty_>
    void bind_simple_property(const std::string &key, Ty_ &value, const PropAccess access = PropAccess::kPublic) {
        property_table_.bind_simple_property(key, value, {}, "", access);
    }

    void bind_simple_property(const std::string &key, bool &value, const PropAccess access = PropAccess::kPublic) {
        property_table_.bind_simple_property(key, value, {}, "", access);
    }

    template<class Ty_>
    void bind_simple_property(const std::string &key, Ty_ &value, int minimum, int maximum,
                              const std::string &desc = {}, const PropAccess access = PropAccess::kPublic) {
        property_table_.bind_simple_property(key, value, {}, minimum, maximum, "", access);
    }

    void set_spec_data(std::map<std::string, std::string> value) { node_spec_data_ = std::move(value); }
    auto &get_spec_data() const { return node_spec_data_; }

    void input_endpoint_increment() { ++input_endpoint_count_; }
    size_t get_input_endpoint_count() const { return input_endpoint_count_; }

public:
    std::ostream &debug_print_output_links(std::ostream &oss) const;

protected:
    virtual result_for<endpoint::DataFeatures> try_set_endpoint_data_features_(
        endpoint::Type type,
        int endpoint,
        const endpoint::DataFeatures &description) {
        return get_endpoint_features(type, endpoint);
    }

protected:
    virtual void on_setup() {};
    virtual void on_input_message(int endpoint, const MessagePtr &message) {
        _dbg_print_in_msg_hdr(std::cout, endpoint, message)
            << message->to_string()
            << std::endl;
        push_output_endpoint(endpoint, message);
    }
    virtual result_for<endpoint::DataFeatures> on_query_endpoint(endpoint::Type type, int endpoint) const = 0;

protected:
    void handle_message(int endpoint, const MessagePtr &message) {
        msg_stat_info_.in_inc(endpoint);
        on_input_message(endpoint, message);
    }
    std::ostream &_dbg_print_in_msg_hdr(std::ostream &oss, int endpoint, const MessagePtr &message) const {

        oss << "[ IN-" << endpoint << "][" << std::setw(12) << std::left
            << get_endpoint_features(endpoint::Type::Input, endpoint).result.data_features
            << std::right
            << "] ";
        oss << name() << (name_.empty() ? "" : " ")
            << "<" << message->name() << "> ";
        return oss;
    }

    // void set_name_(const std::string &name) { name_ = name; }

    /**
     * @brief 退出当前所在的Runner
     * @param exit_code
     */
    void quit_runner_(TaskErrorCode exit_code) { dispatch_(std::weak_ptr<NodeAny>(), (int)exit_code, nullptr); }

private:
    struct node_msg_stat_info {
        std::unordered_map<int, uint32_t> output_msg_nums;
        std::unordered_map<int, uint32_t> input_msg_nums;

        static void inc_var(int port, std::unordered_map<int, uint32_t> &vars) {
            auto iter = vars.find(port);
            if (iter != vars.end()) {
                iter->second++;
            } else {
                vars[port] = 1;
            }
        }
        void in_inc(int port) { inc_var(port, input_msg_nums); }
        void out_inc(int port) { inc_var(port, output_msg_nums); }
    };

private:
    Runner::MessageDispatch dispatch_;                              // 当前节点入口消息缓冲函数
    std::weak_ptr<Runner> node_runner_;                             // 当前节点所绑定的Runner
    std::string name_;                                              // 当前节点名
    std::string type_;                                              // 当前节点类型
    std::unique_ptr<NodeOutputManager> node_output_manager_;        // 输出节点链接配置
    NodePropertyTable property_table_;                              // 属性配置表(key-value)
    mutable std::mutex mutex_;                                      // public API 访问锁

    friend class Runner;

    std::pair<int, std::map<int, std::string>> node_logs_{1, {}};   // 节点状态及日志
    node_msg_stat_info msg_stat_info_;                              // 节点消息输入输出统计

    size_t input_endpoint_count_{0};                                // 输入端口数量

protected:
    std::map<std::string, std::string> node_spec_data_;             // 运行时传递参数
};

class Bridge : public NodeAny {
public:
    using NodeAny::NodeAny;
    ~Bridge() override { spdlog::debug("destructor: {}, {}", utils::get_class_name(this), name()); }

protected:
    void on_input_message(int endpoint, const MessagePtr &message) override {
        NodeAny::on_input_message(endpoint, message);
    }

    result_for<endpoint::DataFeatures> on_query_endpoint(
        endpoint::Type type,
        int endpoint) const override {
        return {((endpoint == 0) && is_valid(data_features_)), data_features_};
    }

    result_for<endpoint::DataFeatures> try_set_endpoint_data_features_(
        endpoint::Type type,
        int endpoint,
        const endpoint::DataFeatures &data_features) override {
        data_features_ = data_features;
        return get_endpoint_features(type, endpoint);
    }

private:
    endpoint::DataFeatures data_features_;
};
}
}

#endif //FFMPEG_WRAPPER_SRC_RUNNABLE_NODE_HPP_
