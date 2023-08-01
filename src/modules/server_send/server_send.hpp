//
// Created by cc on 2022/1/17.
//

#ifndef FFMPEG_WRAPPER_SRC_MODULES_SERVER_SEND_SERVER_SEND_HPP_
#define FFMPEG_WRAPPER_SRC_MODULES_SERVER_SEND_SERVER_SEND_HPP_

#include "common_basic/basic_macro_concepts.hpp"
#include "common_basic/basic_singleton.hpp"
#include "hv/WebSocketServer.h"
#include <memory>

namespace gddi {
class server_send : public basic_singleton<server_send> {
    friend class basic_singleton<server_send>;

public:
    NO_MOVABLE(server_send);
    NO_COPYABLE(server_send);

    void push_string_message(const std::string &message);
    void push_binary_message(const char *data, int size, const std::string &channel = {});
    void push_log(const std::string &log_message);

    WebSocketService &get_websocket_service();
private:
    server_send();

private:
    class server_send_impl;
    std::shared_ptr<server_send_impl> priv_;
};
}

#endif //FFMPEG_WRAPPER_SRC_MODULES_SERVER_SEND_SERVER_SEND_HPP_
