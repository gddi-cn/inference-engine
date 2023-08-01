//
// Created by cc on 2021/12/1.
//

#ifndef FFMPEG_WRAPPER_SRC_MODULES_ZMQ_CV_POST_CPPZMQ_INSTANCE_HPP_
#define FFMPEG_WRAPPER_SRC_MODULES_ZMQ_CV_POST_CPPZMQ_INSTANCE_HPP_
#include <memory>
#include <mutex>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#ifndef CPPZMQ_MAX_IO_THREAD
#define CPPZMQ_MAX_IO_THREAD    1
#endif

namespace gddi {

class cppzmq {
private:
    zmq::context_t ctx_;

private:
    cppzmq() : ctx_(CPPZMQ_MAX_IO_THREAD) {};
public:
    cppzmq(const cppzmq &) = delete;
    cppzmq(cppzmq &&) = delete;
    cppzmq &operator=(const cppzmq &) = delete;

    ~cppzmq() = default;

    static cppzmq &instance() {
        static std::mutex mutex_;
        static std::shared_ptr<cppzmq> instance_;

        std::lock_guard<std::mutex> lock_guard(mutex_);

        if (instance_ == nullptr) {
            instance_ = std::shared_ptr<cppzmq>(new cppzmq);
        }
        return *instance_;
    }

    zmq::context_t &get() { return ctx_; }
    const zmq::context_t &get() const { return ctx_; }
};
} //gddi

#endif //FFMPEG_WRAPPER_SRC_MODULES_ZMQ_CV_POST_CPPZMQ_INSTANCE_HPP_
