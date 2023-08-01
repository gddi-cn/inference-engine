//
// Created by cc on 2021/11/4.
//

#ifndef FFMPEG_WRAPPER_SRC_NODES_NODE_ANY_BASIC_HPP_
#define FFMPEG_WRAPPER_SRC_NODES_NODE_ANY_BASIC_HPP_
#include "runnable_node.hpp"
#include "message_templates.hpp"
#include "utils.hpp"

namespace gddi {
namespace nodes {

template<class TyNode>
class node_any_basic : public ngraph::NodeAny {
public:
    using NodeAny::NodeAny;
    ~node_any_basic() override {
        spdlog::debug("destructor: {}, {}", utils::get_class_name(this), name());
    }

    void print_info(std::ostream &oss) const override {
        auto flags = oss.flags();

        oss << "***name***: *" << this->name() << "*\n";
        oss << std::right;

        if (!input_endpoints_.empty()) {
            oss << "* ***input endpoints***\n";
        }
        for (size_t i = 0; i < input_endpoints_.size(); i++) {
            oss << "\t> * `[" << std::setw(2) << i << " ]` `"
                << input_endpoints_[i].first.data_features << "`" << std::endl;
        }

        if (!output_endpoints_.empty()) {
            oss << "* ***output endpoints***\n";
        }
        for (size_t i = 0; i < output_endpoints_.size(); i++) {
            oss << "\t> * `[" << std::setw(2) << i << " ]` `"
                << output_endpoints_[i].data_features << "`" << std::endl;
        }

        int i = 0;

        if (!properties().empty()) {
            oss << "* ***node properties***\n";
            for (auto &iter : properties().items()) {
                auto &key = iter.first;
                auto &entry = iter.second;
                oss << "\t * `" << key << "`\n";
                i++;
                oss << "\t\t> `" << entry << "`" << std::endl;
            }
        }

        oss.flags(flags);
    }

public:
    template<class Message_>
    using message_pipe = std::function<void(const std::shared_ptr<Message_> &)>;

protected:
    template<class Message_>
    message_pipe<Message_>
    register_output_message_(const ngraph::endpoint::DataFeatures &data_feature = {}) {
        int endpoint_id = (int)output_endpoints_.size();
        output_endpoints_.push_back(data_feature.data_features.empty() ? gen_data_features<Message_>() : data_feature);
        return [=](const std::shared_ptr<Message_> &message) { push_output_endpoint(endpoint_id, message); };
    }

    template<class Message_, class Class_>
    message_pipe<Message_>
    register_input_message_handler_(const ngraph::endpoint::DataFeatures &data_feature,
                                    void (Class_::* handler)(const std::shared_ptr<Message_> &),
                                    Class_ *this_ptr) {
        int endpoint_id = (int)input_endpoints_.size();
        input_endpoints_.emplace_back(data_feature, [handler, this_ptr](const ngraph::MessagePtr &message) {
            auto msg = std::dynamic_pointer_cast<Message_>(message);
            if (msg) { (*this_ptr.*handler)(msg); }
        });
        return [=](const std::shared_ptr<Message_> &message) { push_input_endpoint(endpoint_id, message); };
    }

    template<class Message_>
    message_pipe<Message_>
    register_input_message_handler_(const ngraph::endpoint::DataFeatures &data_feature,
                                    const std::function<void(const std::shared_ptr<Message_> &)> &handler) {
        int endpoint_id = (int)input_endpoints_.size();
        input_endpoints_.emplace_back(data_feature, [handler](const ngraph::MessagePtr &message) {

            auto msg = std::dynamic_pointer_cast<Message_>(message);
            if (msg) { handler(msg); }
        });
        return [=](const std::shared_ptr<Message_> &message) { push_input_endpoint(endpoint_id, message); };
    }

    template<class Message_, class Class_>
    message_pipe<Message_>
    register_input_message_handler_(void (Class_::* handler)(const std::shared_ptr<Message_> &), Class_ *this_ptr) {
        return register_input_message_handler_(gen_data_features<Message_>(), handler, this_ptr);
    }

    template<class Message_>
    message_pipe<Message_>
    register_input_message_handler_(const std::function<void(const std::shared_ptr<Message_> &)> &handler) {
        return register_input_message_handler_(gen_data_features<Message_>(), handler);
    }

protected:
    void on_input_message(int endpoint, const ngraph::MessagePtr &message) override {
        if (endpoint < (int)input_endpoints_.size()) {
            input_endpoints_[endpoint].second(message);
        } else {
            ngraph::NodeAny::on_input_message(endpoint, message);
        }
    }

    result_for<ngraph::endpoint::DataFeatures> on_query_endpoint(
        ngraph::endpoint::Type type, int endpoint) const override {
        if (type == ngraph::endpoint::Type::Input) {
            if (endpoint < (int)input_endpoints_.size()) {
                return {true, input_endpoints_[endpoint].first};
            }
        } else if (type == ngraph::endpoint::Type::Output) {
            if (endpoint < (int)output_endpoints_.size()) {
                return {true, output_endpoints_[endpoint]};
            }
        }
        return {false, {}};
    }

private:
    std::vector<std::pair<ngraph::endpoint::DataFeatures, std::function<void(const ngraph::MessagePtr &)>>>
        input_endpoints_;
    std::vector<ngraph::endpoint::DataFeatures> output_endpoints_;
};

}
}

#endif //FFMPEG_WRAPPER_SRC_NODES_NODE_ANY_BASIC_HPP_
