//
// Created by cc on 2022/6/10.
//

#ifndef YACD_SRC_EXPERIMENTAL_MEMORY_BASIC_MANAGED_SHARED_HPP_
#define YACD_SRC_EXPERIMENTAL_MEMORY_BASIC_MANAGED_SHARED_HPP_
#include <memory>
#include <mutex>
#include <blockingconcurrentqueue.h>
#include <unordered_map>
#include <functional>
#include <spdlog/spdlog.h>
#include <common_basic/thread_dbg_utils.hpp>

namespace yacd::experimental::memory {

class basic_managed_shared {
private:
    struct managed_object_context_t {
        std::string type_name;
        size_t create_n{0};
    };

public:
    basic_managed_shared(const basic_managed_shared &) = delete;
    basic_managed_shared(basic_managed_shared &&) = delete;
    basic_managed_shared &operator=(const basic_managed_shared &) = delete;
    basic_managed_shared &operator=(basic_managed_shared &&) = delete;

public:
    basic_managed_shared() : quit_deleter_(false) {
        deleter_thread_ = std::thread([this] { deleter_daemon(); });
    }

    virtual ~basic_managed_shared() {
        managed_map_mutex_.lock();
        quit_deleter_ = true;
        managed_map_mutex_.unlock();

        deleter_queue_.enqueue(nullptr);
        if (deleter_thread_.joinable()) {
            deleter_thread_.join();
        }
    }

    template<class MessageType_>
    static MessageType_ *creator_for() { return new MessageType_; }
    template<class MessageType_, class... Args>
    static MessageType_ *creator_for(Args &&... args) { return new MessageType_(std::forward<Args>(args)...); }

    template<class MessageType_>
    std::function<void(MessageType_ *)> deleter_for() {
        return [this](MessageType_ *p) { deleter_queue_.enqueue([p] { delete p; }); };
    }

    template<class MessageType_>
    std::shared_ptr<MessageType_> create_object() {
        std::lock_guard<std::mutex> lock_guard(managed_map_mutex_);

        if (quit_deleter_) { return nullptr; }

        create_util<MessageType_>();

        return std::shared_ptr<MessageType_>(creator_for<MessageType_>(),
                                             deleter_for<MessageType_>());
    }

    template<class MessageType_, class... Args>
    std::shared_ptr<MessageType_> create_object(Args &&... args) {
        std::lock_guard<std::mutex> lock_guard(managed_map_mutex_);

        if (quit_deleter_) { return nullptr; }

        create_util<MessageType_>();

        return std::shared_ptr<MessageType_>(creator_for<MessageType_>(std::forward<Args>(args)...),
                                             deleter_for<MessageType_>());
    }

private:
    template<class MessageType_>
    void create_util() {
        auto &ctx = get_object_context_<MessageType_>();
        ctx.create_n++;
        stat_total_created_++;
    }

private:
    template<class MessageType_>
    managed_object_context_t &get_object_context_() const {
        auto hash_code = typeid(MessageType_).hash_code();
        auto iter = managed_map_.find(hash_code);
        if (iter != managed_map_.end()) {
            return iter->second;
        }

        // new message type, init ctx;
        auto &ctx = managed_map_[hash_code];
        ctx.type_name = typeid(MessageType_).name();
        return ctx;
    }

    void deleter_daemon() {
        std::function<void()> deleter_functor;
        size_t deleted_n = 0;
        gddi::thread_utils::set_cur_thread_name(__FUNCTION__);

        while (true) {
            deleter_queue_.wait_dequeue(deleter_functor);
            if (deleter_functor) {
                deleter_functor();
                deleted_n++;
            }

            if (quit_deleter_ &&
                deleted_n >= stat_total_created_) {
                break;
            }
        }

        if (deleted_n) {
            spdlog::info("Still not direct released object COUNT: {}\n", stat_total_created_ - deleted_n);
            spdlog::info("Total object for managed COUNT: {}\n", stat_total_created_);

            for (const auto &ctx: managed_map_) {
                spdlog::info("{:20}: {}\n", ctx.second.type_name, ctx.second.create_n);
            }
        }
    }

private:
    mutable std::mutex managed_map_mutex_;
    mutable std::unordered_map<size_t, managed_object_context_t> managed_map_;
    moodycamel::BlockingConcurrentQueue<std::function<void()>> deleter_queue_;
    std::thread deleter_thread_;
    size_t stat_total_created_{0};
    bool quit_deleter_;
};
}

#endif //YACD_SRC_EXPERIMENTAL_MEMORY_BASIC_MANAGED_SHARED_HPP_
