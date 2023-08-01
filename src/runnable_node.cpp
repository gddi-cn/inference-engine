//
// Created by cc on 2021/11/1.
//

#include "runnable_node.hpp"
#include <common_basic/thread_dbg_utils.hpp>

namespace gddi {
namespace ngraph {
bool endpoint::is_connectable(const endpoint::DataFeatures &from,
                              const endpoint::DataFeatures &to) {
    return (!from.data_features.empty()) && (from.data_features == to.data_features);
}
bool endpoint::is_valid(const endpoint::DataFeatures &data_features) {
    return !data_features.data_features.empty();
}

class RunnerEventWatcher {
public:
    ~RunnerEventWatcher() {
        stop();
        std::cout << "destructor: " << utils::get_class_name(this);
        if (watcher_thread_.joinable()) { watcher_thread_.join(); }
    }

    static RunnerEventWatcher &get_instance() {
        static std::mutex mutex;
        static std::shared_ptr<RunnerEventWatcher> instance{nullptr};

        std::lock_guard<std::mutex> lock_guard(mutex);

        if (instance == nullptr) {
            instance = std::shared_ptr<RunnerEventWatcher>(new RunnerEventWatcher);
        }
        return *instance;
    }

    void stop() { action_queue_.enqueue({true, nullptr}); }
    void push_action(const std::function<void()> &action) {
        action_queue_.enqueue({false, action});
    }

private:
    struct Event {
        bool stop_signal{};
        std::function<void()> event_action;
    };
    RunnerEventWatcher() {
        watcher_thread_ = std::thread([this] { run(); });
    }

    void run() {
        Event event;
        while (true) {
            action_queue_.wait_dequeue(event);
            if (event.stop_signal) {
                break;
            } else {
                try {
                    event.event_action();
                } catch (std::exception &exception) {
                    std::cerr << "RunnerEventWatcher: " << exception.what() << std::endl;
                }
            }
        }
    }

    std::thread watcher_thread_;
    moodycamel::BlockingConcurrentQueue<Event> action_queue_;
};

void Runner::_run() {
    RunnerMessage message;
    spdlog::debug("Runner started! {}", thread_.native_handle());

    auto init_nodes = [this] {
        auto &&nodes_to_init = attached_node_manager_->take_attach_nodes();
        if (!nodes_to_init.empty()) {
            for (const auto &item : nodes_to_init) { item.second.on_setup(); }
        }
    };
    int quit_code_ = 0;
    gddi::thread_utils::set_cur_thread_name(std::string("rn|" + name_));

    // 1. init setup all node
    init_nodes();

    // 2. loop the queued message
    while (true) {
        message_queue_.wait_dequeue(message);

        // DEBUG, for queued message is too many
        auto queued_message_num = message_queue_.size_approx();
        if (queued_message_num > 5) {
            spdlog::warn("too many message in queue! {}: {}", name_, queued_message_num);
        }


        // take and init all
        if (message.is_echo) {
            init_nodes();
            continue;
        }

        // quit loop if read empty message
        if (message.message == nullptr) {
            quit_code_ = message.port_in;
            break;
        }

        // process message
        auto receiver = message.receiver.lock();
        message.message->message_queued_size = message_queue_.size_approx();
        if (receiver) { receiver->handle_message(message.port_in, message.message); }
    }
    spdlog::debug("Runner loop over: {}", name_);

    if (on_quit_) {
        auto quit_tmp_ = on_quit_;
        RunnerEventWatcher::get_instance().push_action([quit_tmp_, quit_code_] {
            /**
             * 找不到 BUG 请打开注释
             * 
             * std::this_thread::sleep_for(std::chrono::milliseconds(5));
             */
            quit_tmp_(quit_code_);
        });
    }
}

void Runner::dispatch_message(const std::weak_ptr<NodeAny> &node, int endpoint,
                              const MessagePtr &message) {
    // std::cout << "[Queued] " << node->name() << ": " << message->name() << ", " << message->to_string() << std::endl;
    message_queue_.enqueue({node, endpoint, message});
}

class NodeAny::NodeOutputManager {
public:
    struct OutputTarget {
        explicit OutputTarget(int ep, std::weak_ptr<NodeAny> node)
            : target_endpoint(ep), target_node(std::move(node)) {}
        int target_endpoint;
        std::string dbg_target_name;
        std::string dbg_target_type;
        std::weak_ptr<NodeAny> target_node;
    };
    using OutputTargetList = std::list<OutputTarget>;

public:
    NodeAny *node_;
    mutable std::mutex mutex_;
    std::unordered_map<int, OutputTargetList> output_listeners;// 输出节点链接配置

public:
    explicit NodeOutputManager(NodeAny *node) : node_(node) {}
    ~NodeOutputManager() = default;

    std::ostream &debug_print_output_links(std::ostream &oss) const {
        auto old_flags = oss.flags();

        {
            std::lock_guard<std::mutex> lock_guard(mutex_);// 访问保护
            for (const auto &item : output_listeners) {
                oss << "Output Endpoint: " << item.first << ", Linked: " << item.second.size()
                    << std::endl;
            }
        }

        oss.flags(old_flags);
        return oss;
    }

    bool connect_to(const std::shared_ptr<NodeAny> &target_node, int output_endpoint,
                    int target_endpoint) {
        static std::mutex connect_mutex_;
        std::lock_guard<std::mutex> conn_lock_guard(
            connect_mutex_);// 确保任何时候同时connect的只有一个

        auto output_endpoint_desc =
            node_->get_endpoint_features(endpoint::Type::Output, output_endpoint);
        auto target_endpoint_desc =
            target_node->get_endpoint_features(endpoint::Type::Input, target_endpoint);

        auto connectable = [&]() {
            std::lock_guard<std::mutex> lock_guard(mutex_);// 访问保护

            if (output_endpoint_desc.success && target_endpoint_desc.success) {
                if (is_connectable(output_endpoint_desc.result, target_endpoint_desc.result)) {
                    OutputTarget output_target(target_endpoint, target_node);
                    output_target.dbg_target_name = target_node->name();
                    output_target.dbg_target_type = target_node->type();
                    output_listeners[output_endpoint].push_back(output_target);
                    return true;
                }
            }
            return false;
        };

        // Case 1, 两边都是正常结点
        if (connectable()) { return true; }

        // Case 2.1, 目标节点-端点不存在
        if (output_endpoint_desc.success && !target_endpoint_desc.success) {
            target_endpoint_desc = target_node->try_set_endpoint_data_features_(
                endpoint::Type::Input, target_endpoint, output_endpoint_desc.result);
            if (connectable()) { return true; }
        }

        // Case 2.2, 当前节点-端点不存在
        if (!output_endpoint_desc.success && target_endpoint_desc.success) {
            output_endpoint_desc = node_->try_set_endpoint_data_features_(
                endpoint::Type::Output, output_endpoint, target_endpoint_desc.result);
            if (connectable()) { return true; }
        }

        return false;
    }

    void raise_next(int endpoint, const MessagePtr &message) {
        std::lock_guard<std::mutex> lock_guard(mutex_);// 访问保护

        auto &&iter = output_listeners.find(endpoint);
        if (iter != output_listeners.end()) {
            auto &output_target_list = iter->second;
            auto list_iter = output_target_list.begin();

            // 循环发送消息，删除已经失效的消息节点
            int invalid_node_count = 0;
            int cur_size = (int)output_target_list.size();
            for (; list_iter != output_target_list.end();) {
                auto shared_p = list_iter->target_node.lock();
                if (shared_p) {
                    shared_p->push_input_endpoint(list_iter->target_endpoint, message);
                    list_iter++;
                } else {
                    // DEBUG info
                    spdlog::debug("***===*** Node target lost, from: {}({}) => {}({})",
                                  node_->type(), node_->name(), list_iter->dbg_target_type,
                                  list_iter->dbg_target_name, output_target_list.size() - 1);
                    list_iter = output_target_list.erase(list_iter);
                    invalid_node_count++;
                }
            }

            if (invalid_node_count) {
                spdlog::debug("======================INVALID OUTPUT: {}, {} to {} "
                              "====================================",
                              invalid_node_count, cur_size, (int)output_target_list.size());
            }
        }
    }

    void get_connections(std::vector<NodeConnectionLine> &connections) const {
        std::lock_guard<std::mutex> lock_guard(mutex_);// 访问保护

        for (const auto &cc : output_listeners) {
            NodeConnectionLine connection_line{};
            connection_line.from_ = node_;
            connection_line.from_ep_ = cc.first;

            for (const auto &c : cc.second) {
                auto target_ = c.target_node.lock();
                if (target_) {
                    connection_line.to_ = target_.get();
                    connection_line.to_ep_ = c.target_endpoint;
                    connections.push_back(connection_line);
                }
            }
        }
    }
};

NodeAny::NodeAny(std::string name)
    : name_(std::move(name)), node_output_manager_(new NodeOutputManager(this)) {
    dispatch_ = [this](const std::weak_ptr<NodeAny> &node, int endpoint,
                       const MessagePtr &message) {
        std::cout << "======================  MESSAGE LOST: " << this << ", " << endpoint
                  << std::endl;
    };
}

NodeAny::~NodeAny() = default;

bool NodeAny::connect_to(const std::shared_ptr<NodeAny> &target_node, int output_endpoint,
                         int target_endpoint) {
    return node_output_manager_->connect_to(target_node, output_endpoint, target_endpoint);
}

void NodeAny::push_output_endpoint(int endpoint, const MessagePtr &message) {
    node_output_manager_->raise_next(endpoint, message);
}

void NodeAny::bind_runner(const std::shared_ptr<Runner> &runner) {
    mutex_.lock();
    auto weak = weak_from_this();
    mutex_.unlock();

    auto dispatch = runner->attach_node(this, [weak] {
        auto s_ptr = weak.lock();
        if (s_ptr) { s_ptr->on_setup(); }
    });

    mutex_.lock();
    dispatch_ = std::move(dispatch);
    node_runner_ = runner;
    mutex_.unlock();
}

std::ostream &NodeAny::debug_print_output_links(std::ostream &oss) const {
    return node_output_manager_->debug_print_output_links(oss);
}

void NodeAny::get_connections(std::vector<NodeConnectionLine> &connections) const {
    return node_output_manager_->get_connections(connections);
}

///////////////////////////////////////////////////////////////////////////////////
}// namespace ngraph
}// namespace gddi