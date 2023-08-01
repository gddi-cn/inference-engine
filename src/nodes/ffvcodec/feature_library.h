#ifndef _FEATURE_LIBRARY_H_
#define _FEATURE_LIBRARY_H_

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"

namespace gddi {
namespace nodes {

class FeatureLibrary_v2 : public node_any_basic<FeatureLibrary_v2> {

public:
    explicit FeatureLibrary_v2(std::string name) : node_any_basic(std::move(name)) {
        output_image_ = register_output_message_<msgs::cv_frame>();

        bind_simple_property("task_name", task_name_, ngraph::PropAccess::kPrivate);
        bind_simple_property("folder_path", folder_path_, "特征库目录");

        register_input_message_handler_(&FeatureLibrary_v2::on_response, this);
    }

    ~FeatureLibrary_v2();

protected:
    void on_setup() override;

private:
    void on_response(const std::shared_ptr<msgs::cv_frame> &frame);

    void get_image_files(const std::string &path);

    void load_feature_v1(const std::string &path);
    void save_feature_v1(const std::string &path);

private:
    const std::string path_suffix_{"/home/data/"};

    std::string task_name_;
    std::string folder_path_;
    ImagePool mem_pool_;

    std::map<std::string, std::vector<uint8_t>> file_list_;
    std::vector<FeatureInfo_v1> feature_info_;

    message_pipe<msgs::cv_frame> output_image_;
};

}// namespace nodes
}// namespace gddi

#endif// _FEATURE_LIBRARY_H_
