#include "tsing_inference.h"
#include "api/infer_api.h"
#include "core/mem/buf_surface_impl.h"
#include "core/mem/buf_surface_util.h"
#include "core/result_def.h"
#include "ts_comm_vdec.h"
#include <future>
#include <memory>
#include <vector>

namespace gddi {
namespace algo {

struct TsingInference::Impl {
    std::unique_ptr<gddeploy::InferAPI> alg_impl;
    AlgoType algo_type{AlgoType::kUndefined};

    std::unique_ptr<BufSurfaceParams> surf_params;
    std::shared_ptr<gddeploy::BufSurfaceWrapper> surf_ptr;
};

TsingInference::TsingInference() : impl_(std::make_unique<TsingInference::Impl>()) {
    impl_->surf_params = std::make_unique<BufSurfaceParams>();
    impl_->surf_ptr = std::make_shared<gddeploy::BufSurfaceWrapper>(new BufSurface, false);
    impl_->surf_ptr->GetBufSurface()->surface_list = impl_->surf_params.get();
}

TsingInference::~TsingInference() {
    if (impl_->alg_impl) { impl_->alg_impl->WaitTaskDone(); }
}

bool TsingInference::init(const ModParms &parms) {
    impl_->alg_impl = std::make_unique<gddeploy::InferAPI>();
    impl_->alg_impl->Init("", parms.mod_path, "", gddeploy::ENUM_API_TYPE::ENUM_API_SESSION_API);

    auto model_type = impl_->alg_impl->GetModelType();
    if (model_type == "detection") {
        impl_->algo_type = AlgoType::kDetection;
    } else if (model_type == "classification") {
        impl_->algo_type = AlgoType::kClassification;
    } else if (model_type == "pose") {
        impl_->algo_type = AlgoType::kPose;
    } else if (model_type == "segmentation") {
        impl_->algo_type = AlgoType::kSegmentation;
    } else if (model_type == "action") {
        impl_->algo_type = AlgoType::kAction;
    } else if (model_type == "ocr_det") {
        impl_->algo_type = AlgoType::kOCR_DET;
    } else if (model_type == "ocr_rec") {
        impl_->algo_type = AlgoType::kOCR_REC;
    } else {
        throw std::runtime_error("Undefined model type.");
    }

    AbstractAlgo::init(parms, AlgoType::kDetection, impl_->alg_impl->GetLabels());

    return true;
}

void TsingInference::inference(const std::string &task_name, const std::shared_ptr<nodes::FrameInfo> &info,
                               const InferType type) {

    impl_->surf_ptr->GetBufSurface()->surface_list[0].width = info->src_frame->data->width;
    impl_->surf_ptr->GetBufSurface()->surface_list[0].height = info->src_frame->data->height;
    impl_->surf_ptr->GetBufSurface()->surface_list[0].color_format = GDDEPLOY_BUF_COLOR_FORMAT_NV12;
    impl_->surf_ptr->GetBufSurface()->surface_list[0].pitch = 1;
    impl_->surf_ptr->GetBufSurface()->surface_list[0].data_ptr = info->src_frame->data->buf[0]->data;

    gddeploy::PackagePtr in = gddeploy::Package::Create(1);
    in->data[0]->Set(impl_->surf_ptr);
    in->data[0]->SetUserData(info->infer_frame_idx);

    impl_->alg_impl->InferAsync(
        in, [this](gddeploy::Status status, gddeploy::PackagePtr data, gddeploy::any user_data) {
            std::vector<algo::AlgoOutput> vec_output;
            if (data->data[0]->HasMetaValue()) {
                auto result = data->data[0]->GetMetaData<gddeploy::InferResult>();
                for (auto result_type : result.result_type) {
                    if (result_type == gddeploy::GDD_RESULT_TYPE_DETECT) {
                        for (uint32_t i = 0; i < result.detect_result.detect_imgs.size(); i++) {
                            for (auto &obj : result.detect_result.detect_imgs[i].detect_objs) {
                                vec_output.emplace_back(
                                    AlgoOutput{.class_id = obj.class_id,
                                               .prob = obj.score,
                                               .box = {obj.bbox.x, obj.bbox.y, obj.bbox.w, obj.bbox.h}});
                            }
                        }
                    }
                }

                if (infer_callback_) {
                    infer_callback_(data->data[0]->GetUserData<int64_t>(), impl_->algo_type, vec_output);
                }
            }
        });
}

AlgoType TsingInference::inference(const std::string &task_name, const std::shared_ptr<nodes::FrameInfo> &info,
                                   std::map<int, std::vector<algo::AlgoOutput>> &outputs) {
    int index = 0;
    for (const auto &[target_id, image] : info->ext_info.back().crop_images) {
        impl_->surf_ptr->GetBufSurface()->surface_list[0].width = image.cols;
        impl_->surf_ptr->GetBufSurface()->surface_list[0].height = image.rows;
        impl_->surf_ptr->GetBufSurface()->surface_list[0].color_format = GDDEPLOY_BUF_COLOR_FORMAT_NV12;
        impl_->surf_ptr->GetBufSurface()->surface_list[0].pitch = 1;

        auto pstFrameInfo = (VIDEO_FRAME_INFO_S *)impl_->surf_ptr->GetBufSurface()->surface_list[0].data_ptr;
        pstFrameInfo->stVFrame.u64VirAddr[0] = (TS_U64)&image.data;

        gddeploy::PackagePtr in = gddeploy::Package::Create(1);
        gddeploy::PackagePtr out = gddeploy::Package::Create(1);
        in->data[0]->Set(impl_->surf_ptr);

        impl_->alg_impl->InferSync(in, out);
        if (out->data[0]->HasMetaValue()) {
            auto result = out->data[0]->GetMetaData<gddeploy::InferResult>();
            for (auto result_type : result.result_type) {
                if (result_type == gddeploy::GDD_RESULT_TYPE_DETECT) {
                    for (uint32_t i = 0; i < result.detect_result.detect_imgs.size(); i++) {
                        for (auto &obj : result.detect_result.detect_imgs[i].detect_objs) {
                            obj.bbox.x += info->ext_info.back().crop_rects.at(target_id).x;
                            obj.bbox.y += info->ext_info.back().crop_rects.at(target_id).y;
                            outputs[target_id].emplace_back(
                                AlgoOutput{.class_id = obj.class_id,
                                           .prob = obj.score,
                                           .box = {obj.bbox.x, obj.bbox.y, obj.bbox.w, obj.bbox.h}});
                        }
                    }
                }
            }
        }
    }

    return AlgoType::kDetection;
}

}// namespace algo
}// namespace gddi