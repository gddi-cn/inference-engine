//
// Created by cc on 2021/11/3.
//

#ifndef FFMPEG_WRAPPER_SRC_NODES_MESSAGE_TEMPLATES_HPP_
#define FFMPEG_WRAPPER_SRC_NODES_MESSAGE_TEMPLATES_HPP_
#include <type_traits>
#include <cstdlib>
#include <algorithm>
#include "runnable_node.hpp"
#include "utils.hpp"

namespace gddi {
template<class SimpleMessage_>
class simple_message : public ngraph::Message {
public:
    simple_message() = default;
    explicit simple_message(SimpleMessage_ simple_message) : message(std::move(simple_message)) {}

    std::string name() const override { return utils::get_class_name(&message); }
    std::string to_string() const override { return utils::fmts(message); }

    SimpleMessage_ message{};

    static std::shared_ptr<simple_message<SimpleMessage_>> make_shared(SimpleMessage_ msg_data) {
        return std::make_shared<simple_message>(std::move(msg_data));
    }

    typedef std::shared_ptr<simple_message> ptr;
    typedef SimpleMessage_ message_value_type;
};

template<class AvStruct, AvStruct *(*alloc_)(const AvStruct *), void(*free_)(AvStruct **)>
class simple_av_message : public simple_message<AvStruct *> {
public:
    explicit simple_av_message(const AvStruct *simple_value)
        : simple_message<AvStruct *>() {
        this->message = alloc_(simple_value);
    }

    ~simple_av_message() override {
        free_(&this->message);
    }
};

template<typename T, typename = void>
struct has_simple_message : std::false_type {
    using message_type = bool;
};

template<typename T>
struct has_simple_message<T, decltype(std::declval<T>().message, void())> : std::true_type {
    using message_type = decltype(std::declval<T>().message);
};

inline std::string rm_namespaces(const std::string &s) {
    auto colons = s.find("::");
    if (colons != std::string::npos) {
        auto ns = s.substr(0, colons);
        if (ns == "std") {
            return s;
        }
        return rm_namespaces(s.substr(colons + 2));
    }
    return s;
}

inline std::string rm_tail_ptr(const std::string &s) {
    auto pos = s.find(" __ptr64");
    return pos != std::string::npos ? s.substr(0, pos) : s;
}

inline std::string rm_head_cls(const std::string &s) {
    auto pos = s.find_first_of(' ');
    return pos != std::string::npos ? s.substr(pos + 1) : s;
}

inline std::string rm_spaces(std::string s) {
    s.erase(std::remove_if(s.begin(), s.end(), isspace), s.end());
    return s;
}

template<class AnyMessageType_>
inline ngraph::endpoint::DataFeatures gen_data_features() {
    auto has_message = has_simple_message<AnyMessageType_>::value;
    auto fullname = has_message
                    ? utils::demangle(typeid(typename has_simple_message<AnyMessageType_>::message_type).name())
                    : utils::demangle(typeid(AnyMessageType_).name());

    return {rm_namespaces(rm_head_cls(rm_tail_ptr(fullname)))};
}

}

#endif //FFMPEG_WRAPPER_SRC_NODES_MESSAGE_TEMPLATES_HPP_
