//
// Created by cc on 2022/1/19.
//

#ifndef FFMPEG_WRAPPER_SRC__INFERENCE_SERVER__WS_NOTIFIER_HPP_
#define FFMPEG_WRAPPER_SRC__INFERENCE_SERVER__WS_NOTIFIER_HPP_

#include <json.hpp>
#include "server_send.hpp"

namespace ws_notifier {

enum class BinaryMessageTypes {
    kImageJpeg,
};

namespace util {
class bytes_packager {
protected:
    struct mem_obj {
        const unsigned char *addr;
        int size;
    };
    std::vector<mem_obj> vars_to_pack_;
public:
    template<class Ty>
    void bind_var(const Ty &var) {
        vars_to_pack_.push_back({(const unsigned char *)&var, sizeof(Ty)});
    }

    void bind_var(const std::vector<unsigned char> &data) {
        vars_to_pack_.push_back({(const unsigned char *)data.data(), (int)data.size()});
    }

    void bind_var(const char *data, int size) {
        vars_to_pack_.push_back({(const unsigned char *)data, size});
    }

    std::vector<unsigned char> pack_all() {
        std::vector<unsigned char> final_data;
        int total_size = 0;
        for (const auto &iter : vars_to_pack_) {
            total_size += iter.size;
        }

        final_data.resize(total_size);
        auto data_ptr = final_data.data();
        for (const auto &iter : vars_to_pack_) {
            memcpy(data_ptr, iter.addr, iter.size);
            data_ptr += iter.size;
        }
        return final_data;
    }
};
}

inline
void push_status(const std::string &what, const nlohmann::json &json = nlohmann::json::object()) {
    auto status_obj = nlohmann::json::object();
    status_obj["type"] = what;
    status_obj["data"] = json;
    gddi::server_send::instance().push_string_message(status_obj.dump());
}

inline
void push_image(const std::string &uuid, const std::vector<unsigned char> &data) {
    util::bytes_packager packager;
    auto msg_type = (int)BinaryMessageTypes::kImageJpeg;
    auto str_size = (int)uuid.size();
    auto img_size = (int)data.size();
    packager.bind_var(msg_type);
    packager.bind_var(str_size);
    packager.bind_var(img_size);
    packager.bind_var(uuid.c_str(), str_size);
    packager.bind_var(data);
    auto &&b_data = packager.pack_all();
    gddi::server_send::instance().push_binary_message((const char *)b_data.data(), (int)b_data.size(), uuid);
}
}

#endif //FFMPEG_WRAPPER_SRC__INFERENCE_SERVER__WS_NOTIFIER_HPP_
