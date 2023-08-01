//
// Created by cc on 2021/11/10.
//

#ifndef FFMPEG_WRAPPER_SRC_NODES_MESSAGE_REF_TIME_HPP_
#define FFMPEG_WRAPPER_SRC_NODES_MESSAGE_REF_TIME_HPP_

#include "runnable_node.hpp"
#include "modules/server_send/ws_notifier.hpp"

namespace gddi {
namespace nodes {

class MessageRefTime_v1 : public ngraph::Bridge {
public:
    explicit MessageRefTime_v1(std::string name)
        : Bridge(std::move(name)) {
        bind_simple_property("print_to_ws", print_to_ws_);
    }

private:
    bool print_to_ws_{false};

protected:
    void on_input_message(int endpoint, const ngraph::MessagePtr &message) override {
        auto cur = std::chrono::high_resolution_clock::now();
        auto diff = cur - message->timestamp.ref.created;
        if (print_to_ws_) {
            auto json = nlohmann::json::object();
            json["value"] = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
            json["index"] = name();
            ws_notifier::push_status("scalar", json);
        } else {
            std::cout << "time process for: " << std::setw(8) << message->timestamp.ref.id
                      << ", " << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << "ms"
                      << std::endl;
        }
    }
};

}
}

#endif //FFMPEG_WRAPPER_SRC_NODES_MESSAGE_REF_TIME_HPP_
