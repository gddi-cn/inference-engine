#include "basic_logs.hpp"
#include "spdlog/spdlog.h"
#include "thread.hpp"
#include <chrono>
#include <condition_variable>
#include <hv/requests.h>
#include <queue>

namespace gddi {
namespace network {

struct EventObject {
    std::string task_name;
    std::string event_id;
    std::string url;
    std::string buffer;
    std::size_t num_of_retries{0};
};

class EventSocket {
public:
    EventSocket(const EventSocket &) = delete;
    EventSocket &operator=(const EventSocket &) = delete;

    static EventSocket &get_instance() {
        static EventSocket handle;
        return handle;
    }

    void post_sync(const std::string &task_name, const std::string &event_id, const std::string &url,
                   const std::string &data) {
        std::lock_guard<std::mutex> glk(mtx_event_cache_);
        event_cache_.emplace(EventObject{task_name, event_id, url, data});
        cv_event_cache_.notify_one();
    }

private:
    EventSocket() {
        thraed_handle_ = std::make_unique<gddi::Thread>(
            [this]() {
                while (running_) {
                    std::unique_lock<std::mutex> ulk(mtx_event_cache_);
                    cv_event_cache_.wait(ulk, [&] { return !running_ || event_cache_.size() > 0; });

                    if (!running_) return;
                    auto element = event_cache_.front();
                    event_cache_.pop();
                    ulk.unlock();

                    HttpRequestPtr req(new HttpRequest);
                    req->method = HTTP_POST;
                    req->url = element.url;
                    req->headers["Content-Type"] = "application/json";
                    req->body = element.buffer;
                    req->timeout = 10;

                    sync_client_.sendAsync(req, [this, element](const HttpResponsePtr &resp) {
                        try {
                            if (resp == nullptr || resp->status_code != 200) {
                                spdlog::error("Post event request failed, task: {}, url: {}, "
                                              "resp-code: {}, resp-msg: {}",
                                              element.task_name, element.url,
                                              resp ? resp->status_code : -1,
                                              resp ? resp->status_message() : "nullptr");
                            }
                        } catch (std::exception &exception) { spdlog::error(exception.what()); }
                    });
                }
            },
            gddi::Thread::DtorAction::join);
        thraed_handle_->start();
    }

    ~EventSocket() { running_ = false; }

private:
    hv::HttpClient sync_client_;

    bool running_{true};
    std::unique_ptr<gddi::Thread> thraed_handle_;

    std::mutex mtx_event_cache_;
    std::condition_variable cv_event_cache_;
    std::queue<EventObject> event_cache_;
};

}// namespace network
}// namespace gddi