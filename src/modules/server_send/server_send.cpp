//
// Created by cc on 2022/1/17.
//
#include <json.hpp>
#include "server_send.hpp"
#include "hv/EventLoop.h"
#include <blockingconcurrentqueue.h>
#include <memory>
#include <thread>
#include <set>
#include <mutex>
#include <iostream>
#include <unordered_map>
#include "basic_logs.hpp"
#include <common_basic/thread_dbg_utils.hpp>

namespace gddi {

struct logs_cache {
    std::list<std::string> history_logs;
    size_t cfg_max_cache = 9999;

    void push_new(const std::string &log) {
        history_logs.push_back(log);
        if (history_logs.size() > cfg_max_cache) {
            history_logs.pop_front();
        }
    }

    std::string build_all() {
        std::vector<std::string> logs_vec;
        logs_vec.reserve(history_logs.size());
        for (const auto &m : history_logs) { logs_vec.push_back(m); }
        auto msg_json = nlohmann::json::object();
        msg_json["type"] = "log-history";
        msg_json["data"] = logs_vec;
        return msg_json.dump();
    }
};

struct web_socket_client_agent {
    WebSocketChannelPtr channel;
    std::string url;

    std::set<std::string> accept_channels;
    std::unordered_map<std::string, std::function<void(const nlohmann::json &)>> handlers_;
    logs_cache logs_cache_;
    std::mutex mutex_;

    bool cfg_push_enabled_ = false;

    web_socket_client_agent() {
        handlers_["book"] = [this](const nlohmann::json &message) {
            for (const auto &item : message["channels"]) { accept_channels.insert(item.get<std::string>()); }
        };

        handlers_["ping"] = [this](const nlohmann::json &message) {
            auto pong = nlohmann::json::object();
            pong["type"] = "pong";
            channel->send(pong.dump());
        };

        handlers_["un-book"] = [this](const nlohmann::json &message) {
            for (const auto &item : message["channels"]) { accept_channels.erase(item.get<std::string>()); }
        };

        handlers_["open-logs"] = [this](const nlohmann::json &message) {
            channel->send(logs_cache_.build_all());
            cfg_push_enabled_ = true;
        };
    }

    void on_message(const std::string &msg) {
        std::lock_guard<std::mutex> lock_guard(mutex_);

        spdlog::debug("RECV({}): {}", url, msg);
        parse_message_(msg);
    }

    void push_log(const std::string &log) {
        std::lock_guard<std::mutex> lock_guard(mutex_);

        logs_cache_.push_new(log);

        if (cfg_push_enabled_) {
            auto msg_json = nlohmann::json::object();
            msg_json["type"] = "log";
            msg_json["data"] = log;
            channel->send(msg_json.dump());
        }
    }

    void parse_message_(const std::string &msg) {
        try {
            auto message = nlohmann::json::parse(msg);
            auto type = message["type"].get<std::string>();
            auto iter = handlers_.find(type);
            if (iter != handlers_.end()) {
                iter->second(message);
            }
        } catch (std::exception &exception) {
            std::cout << "parse error: " << exception.what() << std::endl;
        }
    }
};

struct basic_server_send_message {
    virtual void send(const web_socket_client_agent *client_agent) = 0;
};

struct string_message : public basic_server_send_message {
    std::string data;

    void send(const web_socket_client_agent *client_agent) override {
        client_agent->channel->send(data);
    }
};

struct binary_message : public basic_server_send_message {
    std::vector<char> data;

    void send(const web_socket_client_agent *client_agent) override {
        client_agent->channel->send(data.data(), (int)data.size());
    }
};

struct web_socket_client_agent_manager {
    std::set<web_socket_client_agent *> agents;
    logs_cache logs_cache_global;
    std::mutex mutex;

    void add_agent(web_socket_client_agent *agent) {
        std::lock_guard<std::mutex> lock_guard(mutex);

        agents.insert(agent);
        agent->logs_cache_ = logs_cache_global;
        spdlog::info("add_agent channels: {}, {}", agents.size(), fmt::ptr(this));
    }

    void del_agent(web_socket_client_agent *agent) {
        std::lock_guard<std::mutex> lock_guard(mutex);

        agents.erase(agent);
        spdlog::info("del_agent channels: {}, {}", agents.size(), fmt::ptr(this));
    }

    void send(const std::shared_ptr<basic_server_send_message> &message, const std::string &channel) {
        std::lock_guard<std::mutex> lock_guard(mutex);

        for (const auto &item : agents) {
            if (item->accept_channels.count(channel)) {
                message->send(item);
            }
        }
    }

    void broadcast_ws_log(const std::shared_ptr<basic_server_send_message> &message) {
        std::lock_guard<std::mutex> lock_guard(mutex);

        auto msg_real = std::dynamic_pointer_cast<string_message>(message);
        if (msg_real) {
            // cache global
            logs_cache_global.push_new(msg_real->data);

            // broadcast one to all
            for (const auto &agent : agents) {
                agent->push_log(msg_real->data);
            }
        }
    }
};

class server_send::server_send_impl {
public:
    /**
     * @brief
     */
    enum class DateType {
        kQuit,
        kMessage,
        kLog,
    };

    typedef std::shared_ptr<basic_server_send_message> basic_server_send_message_ptr_ref;

    struct message_t {
        DateType type{DateType::kQuit};
        std::string channel;
        basic_server_send_message_ptr_ref payload;
    };

    WebSocketService ws;
    moodycamel::BlockingConcurrentQueue<message_t> message_queue;
    std::thread message_worker;
    web_socket_client_agent_manager agent_manager;

public:
    server_send_impl() {
        setup_();
    }

    ~server_send_impl() {
        message_queue.enqueue({DateType::kQuit, nullptr});
        if (message_worker.joinable()) {
            message_worker.join();
        }
    }

    void push_string(const std::string &data) {
        auto m = std::make_shared<string_message>();
        m->data = data;
        push_(m);
    }

    void push_binary(const char *data, int size, const std::string &channel) {
        auto m = std::make_shared<binary_message>();
        m->data = std::vector<char>(data, data + size);
        push_(m, channel);
    }

    void push_log(const std::string &message) {
        auto m = std::make_shared<string_message>();
        m->data = message;
        message_queue.enqueue({DateType::kLog, std::string(), m});
    }

protected:
    void push_(const basic_server_send_message_ptr_ref &message, const std::string &channel = {}) {
        message_queue.enqueue({DateType::kMessage, channel, message});
    }

    void setup_() {
        // setup worker
        message_worker = std::thread([this] { run_(); });

        // setup websocket service
        ws.onopen = [=](const WebSocketChannelPtr &channel, const std::string &url) {
            spdlog::info("onopen: GET {}", url);
            auto ctx = channel->newContext<web_socket_client_agent>();
            ctx->channel = channel;
            ctx->url = url;
            agent_manager.add_agent(ctx);
        };
        ws.onmessage = [=](const WebSocketChannelPtr &channel, const std::string &msg) {
            auto ctx = channel->getContext<web_socket_client_agent>();
            ctx->on_message(msg);
        };
        ws.onclose = [=](const WebSocketChannelPtr &channel) {
            auto ctx = channel->getContext<web_socket_client_agent>();
            spdlog::info("onclose: {}", ctx->url);
            agent_manager.del_agent(ctx);
            channel->deleteContext<web_socket_client_agent>();
        };
    }

    void run_() {
        message_t message;
        int64_t proc_n = 0;

        gddi::thread_utils::set_cur_thread_name(std::string("server_send"));
        while (true) {
            message_queue.wait_dequeue(message);

            if (process_(message)) {
                break;
            }
        }
    }

    bool process_(message_t &message) {
        switch (message.type) {
            case DateType::kQuit: return true;
            case DateType::kMessage: dispatch_(message);
                return false;
            case DateType::kLog: log_it_(message);
                return false;
        }
        return false;
    }

    void dispatch_(message_t &message) {
        if (message.payload) {
            agent_manager.send(message.payload, message.channel);
        }
    }

    void log_it_(message_t &message) {
        if (message.payload) {
            agent_manager.broadcast_ws_log(message.payload);
        }
    }
};

server_send::server_send() : priv_(new server_send_impl) {}
WebSocketService &server_send::get_websocket_service() { return priv_->ws; }
void server_send::push_string_message(const std::string &message) { priv_->push_string(message); }
void server_send::push_binary_message(const char *data, int size, const std::string &channel) {
    priv_->push_binary(data, size, channel);
}
void server_send::push_log(const std::string &log_message) {
    priv_->push_log(log_message);
}

}
