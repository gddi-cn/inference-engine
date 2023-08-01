//
// Created by zhdotcai on 4/25/22.
//

#ifndef __ZMQ_SOCKET_H__
#define __ZMQ_SOCKET_H__

#include "basic_logs.hpp"
#include <memory>
#include <string>
#include <zmq.h>
#include <zmq_addon.hpp>

namespace gddi {
namespace network {
class ZmqSocket {
public:
    ZmqSocket(const ZmqSocket &) = delete;
    ZmqSocket &operator=(const ZmqSocket &) = delete;

    static ZmqSocket &get_instance(const std::string &address_) {
        static ZmqSocket handle(address_);
        return handle;
    }

    template<typename T>
    bool send(const std::string &topic, const T &buffer) {
        std::lock_guard<std::mutex> glk(sock_mtx_);
        zmq::multipart_t mp;
        mp.pushmem(buffer.data(), buffer.size());
        mp.pushstr(topic);
        mp.send(*sock_);

        // sock_->send(zmq::message_t(topic.begin(), topic.end()), zmq::send_flags::sndmore);
        // sock_->send(zmq::message_t(buffer.begin(), buffer.end()), zmq::send_flags::none);
        return true;
    }

private:
    explicit ZmqSocket(const std::string &address_) {
        sock_ = std::make_unique<zmq::socket_t>(ctx_, zmq::socket_type::pub);
        sock_->bind(address_);
        spdlog::info("ZMQ Bind Addr: {}", address_);
    }
    ~ZmqSocket() = default;

private:
    zmq::context_t ctx_;
    std::mutex sock_mtx_;
    std::unique_ptr<zmq::socket_t> sock_;
};
}// namespace network
}// namespace gddi

#endif//__ZMQ_SOCKET_H__
