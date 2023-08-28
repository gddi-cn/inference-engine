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
};

TsingInference::TsingInference() : impl_(std::make_unique<TsingInference::Impl>()) {}

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
    // int img_width = info->src_frame->data->width;
    // int img_height = info->src_frame->data->height;
    // int img_format = info->src_frame->data->format;
    // auto img_linesize = info->src_frame->data->linesize;
    // auto img_data = info->src_frame->data->data;

    // BufSurfaceCreateParams params;
    // params.mem_type = GDDEPLOY_BUF_MEM_TS;
    // params.device_id = 0;
    // params.width = img_width;
    // params.height = img_height;
    // params.color_format = GDDEPLOY_BUF_COLOR_FORMAT_NV12;
    // params.force_align_1 = 1;
    // params.bytes_per_pix = 1;
    // params.size = 0;
    // params.batch_size = 1;

    /************** 复现情况一：申请内存并拷贝（推理使用拷贝帧数据）一路25帧卡顿（640x640） **************/
    // auto surf_ptr = std::make_shared<gddeploy::BufSurfaceWrapper>(new BufSurface, true);
    // BufSurface *surf = surf_ptr->GetBufSurface();
    // memset(surf, 0, sizeof(BufSurface));
    // gddeploy::CreateSurface(&params, surf);
    // auto pstFrameInfo = (VIDEO_FRAME_INFO_S *)surf_ptr->GetBufSurface()->surface_list[0].data_ptr;
    // auto size = av_image_get_buffer_size((AVPixelFormat)img_format, img_width, img_height, 32);
    // av_image_copy_to_buffer((uint8_t *)pstFrameInfo->stVFrame.u64VirAddr[0], size, img_data,
    //                         (const int *)info->src_frame->data->linesize, (AVPixelFormat)img_format, img_width,
    //                         img_height, 1);
    // gddeploy::PackagePtr in = gddeploy::Package::Create(1);
    // in->data[0]->Set(surf_ptr);
    // in->data[0]->SetUserData(info->infer_frame_idx);
    /*******************************************************************************************/

    /************* 复现情况二：复用内存空间并拷贝（推理使用拷贝帧数据）一路25帧流畅（640x640）*************/
    // static std::shared_ptr<gddeploy::BufSurfaceWrapper> surf_ptr;
    // if (!surf_ptr) {
    //     surf_ptr = std::make_shared<gddeploy::BufSurfaceWrapper>(new BufSurface, true);
    //     BufSurface *surf = surf_ptr->GetBufSurface();
    //     memset(surf, 0, sizeof(BufSurface));
    //     gddeploy::CreateSurface(&params, surf);
    // }
    // auto pstFrameInfo = (VIDEO_FRAME_INFO_S *)surf_ptr->GetBufSurface()->surface_list[0].data_ptr;
    // auto size = av_image_get_buffer_size((AVPixelFormat)img_format, img_width, img_height, 32);
    // av_image_copy_to_buffer((uint8_t *)pstFrameInfo->stVFrame.u64VirAddr[0], size, img_data,
    //                         (const int *)info->src_frame->data->linesize, (AVPixelFormat)img_format, img_width,
    //                         img_height, 1);
    // gddeploy::PackagePtr in = gddeploy::Package::Create(1);
    // in->data[0]->Set(surf_ptr);
    // in->data[0]->SetUserData(info->infer_frame_idx);
    /*******************************************************************************************/

    /************* 复现情况四：复用内存空间并拷贝（推理使用原始帧数据）一路25帧异常（640x640）*************/
    // static std::shared_ptr<gddeploy::BufSurfaceWrapper> tmp_surf_ptr;
    // if (!tmp_surf_ptr) {
    //     tmp_surf_ptr = std::make_shared<gddeploy::BufSurfaceWrapper>(new BufSurface, true);
    //     BufSurface *surf = tmp_surf_ptr->GetBufSurface();
    //     memset(surf, 0, sizeof(BufSurface));
    //     gddeploy::CreateSurface(&params, surf);
    // }
    // auto pstFrameInfo = (VIDEO_FRAME_INFO_S *)tmp_surf_ptr->GetBufSurface()->surface_list[0].data_ptr;
    // auto size = av_image_get_buffer_size((AVPixelFormat)img_format, img_width, img_height, 32);
    // av_image_copy_to_buffer((uint8_t *)pstFrameInfo->stVFrame.u64VirAddr[0], size, img_data,
    //                         (const int *)info->src_frame->data->linesize, (AVPixelFormat)img_format, img_width,
    //                         img_height, 1);

    // auto buf_surf = std::shared_ptr<gddeploy::BufSurfaceWrapper>(new gddeploy::BufSurfaceWrapper(new BufSurface, false),
    //                                                              [](gddeploy::BufSurfaceWrapper *ptr) {
    //                                                                  delete ptr->GetBufSurface()->surface_list;
    //                                                                  delete ptr->GetBufSurface();
    //                                                                  delete ptr;
    //                                                              });
    // buf_surf->GetBufSurface()->surface_list = new BufSurfaceParams;
    // buf_surf->GetBufSurface()->surface_list[0].width = info->src_frame->data->width;
    // buf_surf->GetBufSurface()->surface_list[0].height = info->src_frame->data->height;
    // buf_surf->GetBufSurface()->surface_list[0].color_format = GDDEPLOY_BUF_COLOR_FORMAT_NV12;
    // buf_surf->GetBufSurface()->surface_list[0].pitch = 1;
    // buf_surf->GetBufSurface()->surface_list[0].data_ptr = info->src_frame->data->buf[0]->data;

    // gddeploy::PackagePtr in = gddeploy::Package::Create(1);
    // in->data[0]->Set(buf_surf);
    // in->data[0]->SetUserData(info->infer_frame_idx);
    /*******************************************************************************************/

    // 正常情况：不拷贝帧数据（推理使用原始帧数据）一路25帧异常
    auto buf_surf = std::shared_ptr<gddeploy::BufSurfaceWrapper>(new gddeploy::BufSurfaceWrapper(new BufSurface, false),
                                                                 [](gddeploy::BufSurfaceWrapper *ptr) {
                                                                     delete ptr->GetBufSurface()->surface_list;
                                                                     delete ptr->GetBufSurface();
                                                                     delete ptr;
                                                                 });
    buf_surf->GetBufSurface()->surface_list = new BufSurfaceParams;
    buf_surf->GetBufSurface()->surface_list[0].width = info->src_frame->data->width;
    buf_surf->GetBufSurface()->surface_list[0].height = info->src_frame->data->height;
    buf_surf->GetBufSurface()->surface_list[0].color_format = GDDEPLOY_BUF_COLOR_FORMAT_NV12;
    buf_surf->GetBufSurface()->surface_list[0].pitch = 1;
    buf_surf->GetBufSurface()->surface_list[0].data_ptr = info->src_frame->data->buf[0]->data;

    gddeploy::PackagePtr in = gddeploy::Package::Create(1);
    in->data[0]->Set(buf_surf);
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

    // infer_callback_(info->infer_frame_idx, impl_->algo_type, {});
}

AlgoType TsingInference::inference(const std::string &task_name, const std::shared_ptr<nodes::FrameInfo> &info,
                                   std::map<int, std::vector<algo::AlgoOutput>> &outputs) {
    int index = 0;
    for (const auto &[target_id, image] : info->ext_info.back().crop_images) {
        auto surf = new BufSurface;
        auto suf_subf_ptr = new gddeploy::BufSurfaceWrapper(surf, false);
        auto buf_surf =
            std::shared_ptr<gddeploy::BufSurfaceWrapper>(suf_subf_ptr, [&surf](gddeploy::BufSurfaceWrapper *ptr) {
                delete ptr->GetBufSurface()->surface_list;
                delete ptr->GetBufSurface();
                delete ptr;
            });
        buf_surf->GetBufSurface()->surface_list = new BufSurfaceParams;
        buf_surf->GetBufSurface()->surface_list[0].width = image.cols;
        buf_surf->GetBufSurface()->surface_list[0].height = image.rows;
        buf_surf->GetBufSurface()->surface_list[0].color_format = GDDEPLOY_BUF_COLOR_FORMAT_NV12;
        buf_surf->GetBufSurface()->surface_list[0].pitch = 1;

        auto pstFrameInfo = (VIDEO_FRAME_INFO_S *)buf_surf->GetBufSurface()->surface_list[0].data_ptr;
        pstFrameInfo->stVFrame.u64VirAddr[0] = (TS_U64)&image.data;

        gddeploy::PackagePtr in = gddeploy::Package::Create(1);
        gddeploy::PackagePtr out = gddeploy::Package::Create(1);
        in->data[0]->Set(buf_surf);

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