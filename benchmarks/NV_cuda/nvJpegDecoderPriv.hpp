/**
 * @file nvJpegDecoderPriv.hpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-10-11
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#pragma once
#include "spdlog/spdlog.h"
#include <chrono>
#include <cuda_runtime_api.h>
#include <iostream>
#include <nvjpeg.h>
#include <string>
#include <vector>

namespace gddi::experiments {

#define CHECK_CUDA(call)                                                                                               \
    {                                                                                                                  \
        cudaError_t _e = (call);                                                                                       \
        if (_e != cudaSuccess) {                                                                                       \
            std::cout << "CUDA Runtime failure: '#" << _e << "' at " << __FILE__ << ":" << __LINE__ << std::endl;      \
            exit(1);                                                                                                   \
        }                                                                                                              \
    }

#define CHECK_NVJPEG(call)                                                                                             \
    {                                                                                                                  \
        nvjpegStatus_t _e = (call);                                                                                    \
        if (_e != NVJPEG_STATUS_SUCCESS) {                                                                             \
            std::cout << "NVJPEG failure: '#" << _e << "' at " << __FILE__ << ":" << __LINE__ << std::endl;            \
            exit(1);                                                                                                   \
        }                                                                                                              \
    }

inline int dev_malloc(void **p, size_t s) { return (int)cudaMalloc(p, s); }

inline int dev_free(void *p) { return (int)cudaFree(p); }

inline int host_malloc(void **p, size_t s, unsigned int f) { return (int)cudaHostAlloc(p, s, f); }

inline int host_free(void *p) { return (int)cudaFreeHost(p); }

typedef std::vector<std::string> FileNames;
typedef std::vector<std::shared_ptr<std::vector<char>>> FileData;

struct decode_params_t {
    int batch_size = 1;
    bool flag_print_subsampling = false;
    bool flag_print_channel_info = false;

    nvjpegJpegState_t nvjpeg_state;
    nvjpegHandle_t nvjpeg_handle;
    cudaStream_t stream;

    // used with decoupled API
    nvjpegJpegState_t nvjpeg_decoupled_state;
    nvjpegBufferPinned_t pinned_buffers[2];// 2 buffers for pipelining
    nvjpegBufferDevice_t device_buffer;
    nvjpegJpegStream_t jpeg_streams[2];//  2 streams for pipelining
    nvjpegDecodeParams_t nvjpeg_decode_params;
    nvjpegJpegDecoder_t nvjpeg_decoder;

    nvjpegOutputFormat_t fmt = NVJPEG_OUTPUT_BGR;// Default to cv::GpuMat

    bool hw_decode_available;
};

inline void release_buffers(std::vector<nvjpegImage_t> &ibuf) {
    for (int i = 0; i < ibuf.size(); i++) {
        for (int c = 0; c < NVJPEG_MAX_COMPONENT; c++)
            if (ibuf[i].channel[c]) CHECK_CUDA(cudaFree(ibuf[i].channel[c]));
    }
}

inline void create_decoupled_api_handles(decode_params_t &params) {

    CHECK_NVJPEG(nvjpegDecoderCreate(params.nvjpeg_handle, NVJPEG_BACKEND_DEFAULT, &params.nvjpeg_decoder));
    CHECK_NVJPEG(nvjpegDecoderStateCreate(params.nvjpeg_handle, params.nvjpeg_decoder, &params.nvjpeg_decoupled_state));

    CHECK_NVJPEG(nvjpegBufferPinnedCreate(params.nvjpeg_handle, NULL, &params.pinned_buffers[0]));
    CHECK_NVJPEG(nvjpegBufferPinnedCreate(params.nvjpeg_handle, NULL, &params.pinned_buffers[1]));
    CHECK_NVJPEG(nvjpegBufferDeviceCreate(params.nvjpeg_handle, NULL, &params.device_buffer));

    CHECK_NVJPEG(nvjpegJpegStreamCreate(params.nvjpeg_handle, &params.jpeg_streams[0]));
    CHECK_NVJPEG(nvjpegJpegStreamCreate(params.nvjpeg_handle, &params.jpeg_streams[1]));

    CHECK_NVJPEG(nvjpegDecodeParamsCreate(params.nvjpeg_handle, &params.nvjpeg_decode_params));
}

inline void destroy_decoupled_api_handles(decode_params_t &params) {
    CHECK_NVJPEG(nvjpegDecodeParamsDestroy(params.nvjpeg_decode_params));
    CHECK_NVJPEG(nvjpegJpegStreamDestroy(params.jpeg_streams[0]));
    CHECK_NVJPEG(nvjpegJpegStreamDestroy(params.jpeg_streams[1]));
    CHECK_NVJPEG(nvjpegBufferPinnedDestroy(params.pinned_buffers[0]));
    CHECK_NVJPEG(nvjpegBufferPinnedDestroy(params.pinned_buffers[1]));
    CHECK_NVJPEG(nvjpegBufferDeviceDestroy(params.device_buffer));
    CHECK_NVJPEG(nvjpegJpegStateDestroy(params.nvjpeg_decoupled_state));
    CHECK_NVJPEG(nvjpegDecoderDestroy(params.nvjpeg_decoder));
}

// prepare buffers for RGBi output format
inline int prepare_buffers(const FileData &file_data, std::vector<size_t> &file_len, std::vector<int> &img_width,
                           std::vector<int> &img_height, std::vector<nvjpegImage_t> &ibuf,
                           std::vector<nvjpegImage_t> &isz, decode_params_t &params) {
    int widths[NVJPEG_MAX_COMPONENT];
    int heights[NVJPEG_MAX_COMPONENT];
    int channels;
    nvjpegChromaSubsampling_t subsampling;

    for (int i = 0; i < file_data.size(); i++) {
        file_len[i] = file_data[i]->size();
        CHECK_NVJPEG(nvjpegGetImageInfo(params.nvjpeg_handle, (const unsigned char *)file_data[i]->data(), file_len[i],
                                        &channels, &subsampling, widths, heights));

        img_width[i] = widths[0];
        img_height[i] = heights[0];

        if (params.flag_print_channel_info) {
            // std::cout << "Processing: " << current_names[i] << std::endl;
            std::cout << "Image is " << channels << " channels." << std::endl;
            for (int c = 0; c < channels; c++) {
                std::cout << "Channel #" << c << " size: " << widths[c] << " x " << heights[c] << std::endl;
            }
        }

        if (params.flag_print_subsampling) {
            switch (subsampling) {
                case NVJPEG_CSS_444: std::cout << "YUV 4:4:4 chroma subsampling" << std::endl; break;
                case NVJPEG_CSS_440: std::cout << "YUV 4:4:0 chroma subsampling" << std::endl; break;
                case NVJPEG_CSS_422: std::cout << "YUV 4:2:2 chroma subsampling" << std::endl; break;
                case NVJPEG_CSS_420: std::cout << "YUV 4:2:0 chroma subsampling" << std::endl; break;
                case NVJPEG_CSS_411: std::cout << "YUV 4:1:1 chroma subsampling" << std::endl; break;
                case NVJPEG_CSS_410: std::cout << "YUV 4:1:0 chroma subsampling" << std::endl; break;
                case NVJPEG_CSS_GRAY: std::cout << "Grayscale JPEG " << std::endl; break;
                case NVJPEG_CSS_410V: break;
                case NVJPEG_CSS_UNKNOWN: std::cout << "Unknown chroma subsampling" << std::endl; return EXIT_FAILURE;
            }
        } else if (subsampling == NVJPEG_CSS_UNKNOWN) {
            return EXIT_FAILURE;
        }

        int mul = 1;
        // in the case of interleaved RGB output, write only to single channel, but
        // 3 samples at once
        if (params.fmt == NVJPEG_OUTPUT_RGBI || params.fmt == NVJPEG_OUTPUT_BGRI) {
            channels = 1;
            mul = 3;
        }
        // in the case of rgb create 3 buffers with sizes of original image
        else if (params.fmt == NVJPEG_OUTPUT_RGB || params.fmt == NVJPEG_OUTPUT_BGR) {
            channels = 3;
            widths[1] = widths[2] = widths[0];
            heights[1] = heights[2] = heights[0];
        }

        // realloc output buffer if required
        for (int c = 0; c < channels; c++) {
            int aw = mul * widths[c];
            int ah = heights[c];
            int sz = aw * ah;
            ibuf[i].pitch[c] = aw;
            if (sz > isz[i].pitch[c]) {
                if (ibuf[i].channel[c]) { CHECK_CUDA(cudaFree(ibuf[i].channel[c])); }
                CHECK_CUDA(cudaMalloc((void **)(&ibuf[i].channel[c]), sz));
                isz[i].pitch[c] = sz;
            }
        }
    }
    return EXIT_SUCCESS;
}

struct CuEventHelper {
    cudaEvent_t startEvent = nullptr;
    cudaEvent_t stopEvent = nullptr;

    CuEventHelper() {
        CHECK_CUDA(cudaEventCreateWithFlags(&startEvent, cudaEventBlockingSync));
        CHECK_CUDA(cudaEventCreateWithFlags(&stopEvent, cudaEventBlockingSync));
    }

    ~CuEventHelper() {
        CHECK_CUDA(cudaEventDestroy(startEvent));
        CHECK_CUDA(cudaEventDestroy(stopEvent));
    }

    void recordStart(cudaStream_t stream) { CHECK_CUDA(cudaEventRecord(startEvent, stream)); }
    void recordStop(cudaStream_t stream) { CHECK_CUDA(cudaEventRecord(stopEvent, stream)); }

    void synchronize(double &decode_gpu_time) {
        float loopTime = 0.0f;
        CHECK_CUDA(cudaEventSynchronize(stopEvent));
        CHECK_CUDA(cudaEventElapsedTime(&loopTime, startEvent, stopEvent));
        decode_gpu_time = 0.001 * static_cast<double>(loopTime);// cudaEventElapsedTime returns milliseconds
    }
};

inline int decode_images_core(const FileData &img_data, const std::vector<size_t> &img_len,
                              std::vector<nvjpegImage_t> &out, decode_params_t &params, CuEventHelper &helper) {
    // CHECK_CUDA(cudaStreamSynchronize(params.stream));
    // cudaEvent_t startEvent = NULL, stopEvent = NULL;
    // float loopTime = 0;
    // CHECK_CUDA(cudaEventCreateWithFlags(&startEvent, cudaEventBlockingSync));
    // CHECK_CUDA(cudaEventCreateWithFlags(&stopEvent, cudaEventBlockingSync));

    std::vector<const unsigned char *> batched_bitstreams;
    std::vector<size_t> batched_bitstreams_size;
    std::vector<nvjpegImage_t> batched_output;

    // bit-streams that batched decode cannot handle
    std::vector<const unsigned char *> otherdecode_bitstreams;
    std::vector<size_t> otherdecode_bitstreams_size;
    std::vector<nvjpegImage_t> otherdecode_output;

    if (params.hw_decode_available) {
        for (int i = 0; i < params.batch_size; i++) {
            // extract bitstream meta data to figure out whether a bit-stream can be decoded
            nvjpegJpegStreamParseHeader(params.nvjpeg_handle, (const unsigned char *)img_data[i]->data(), img_len[i],
                                        params.jpeg_streams[0]);
            int isSupported = -1;
            nvjpegDecodeBatchedSupported(params.nvjpeg_handle, params.jpeg_streams[0], &isSupported);

            if (isSupported == 0) {
                batched_bitstreams.push_back((const unsigned char *)img_data[i]->data());
                batched_bitstreams_size.push_back(img_len[i]);
                batched_output.push_back(out[i]);
            } else {
                otherdecode_bitstreams.push_back((const unsigned char *)img_data[i]->data());
                otherdecode_bitstreams_size.push_back(img_len[i]);
                otherdecode_output.push_back(out[i]);
            }
        }
    } else {
        for (int i = 0; i < params.batch_size; i++) {
            otherdecode_bitstreams.push_back((const unsigned char *)img_data[i]->data());
            otherdecode_bitstreams_size.push_back(img_len[i]);
            otherdecode_output.push_back(out[i]);
        }
    }

    // CHECK_CUDA(cudaEventRecord(startEvent, params.stream));
    helper.recordStart(params.stream);

    if (batched_bitstreams.size() > 0) {
        CHECK_NVJPEG(nvjpegDecodeBatchedInitialize(params.nvjpeg_handle, params.nvjpeg_state, batched_bitstreams.size(),
                                                   1, params.fmt));

        CHECK_NVJPEG(nvjpegDecodeBatched(params.nvjpeg_handle, params.nvjpeg_state, batched_bitstreams.data(),
                                         batched_bitstreams_size.data(), batched_output.data(), params.stream));
    }

    if (otherdecode_bitstreams.size() > 0) {
        CHECK_NVJPEG(nvjpegStateAttachDeviceBuffer(params.nvjpeg_decoupled_state, params.device_buffer));
        int buffer_index = 0;
        CHECK_NVJPEG(nvjpegDecodeParamsSetOutputFormat(params.nvjpeg_decode_params, params.fmt));
        for (int i = 0; i < params.batch_size; i++) {
            CHECK_NVJPEG(nvjpegJpegStreamParse(params.nvjpeg_handle, otherdecode_bitstreams[i],
                                               otherdecode_bitstreams_size[i], 0, 0,
                                               params.jpeg_streams[buffer_index]));

            CHECK_NVJPEG(
                nvjpegStateAttachPinnedBuffer(params.nvjpeg_decoupled_state, params.pinned_buffers[buffer_index]));

            CHECK_NVJPEG(nvjpegDecodeJpegHost(params.nvjpeg_handle, params.nvjpeg_decoder,
                                              params.nvjpeg_decoupled_state, params.nvjpeg_decode_params,
                                              params.jpeg_streams[buffer_index]));

            CHECK_CUDA(cudaStreamSynchronize(params.stream));

            CHECK_NVJPEG(nvjpegDecodeJpegTransferToDevice(params.nvjpeg_handle, params.nvjpeg_decoder,
                                                          params.nvjpeg_decoupled_state,
                                                          params.jpeg_streams[buffer_index], params.stream));

            buffer_index = 1 - buffer_index;// switch pinned buffer in pipeline mode to avoid an extra sync

            CHECK_NVJPEG(nvjpegDecodeJpegDevice(params.nvjpeg_handle, params.nvjpeg_decoder,
                                                params.nvjpeg_decoupled_state, &otherdecode_output[i], params.stream));
        }
    }

    // CHECK_CUDA(cudaEventRecord(stopEvent, params.stream));
    helper.recordStop(params.stream);

    // CHECK_CUDA(cudaEventSynchronize(stopEvent));
    // CHECK_CUDA(cudaEventElapsedTime(&loopTime, startEvent, stopEvent));
    // decode_gpu_time = 0.001 * static_cast<double>(loopTime);// cudaEventElapsedTime returns milliseconds

    return EXIT_SUCCESS;
}

inline int decode_images(const FileData &img_data, const std::vector<size_t> &img_len, std::vector<nvjpegImage_t> &out,
                         decode_params_t &params, double &decode_gpu_time, CuEventHelper &helper) {
    CHECK_CUDA(cudaStreamSynchronize(params.stream));

    auto r = decode_images_core(img_data, img_len, out, params, helper);
    helper.synchronize(decode_gpu_time);
    return r;
}

class NvJpegDecoderInstance {
public:
    NvJpegDecoderInstance() { init_nvjpeg(); }
    ~NvJpegDecoderInstance() { free_nvjpeg(); }

private:
    void free_nvjpeg() {
        // Destroy Stream
        CHECK_CUDA(cudaStreamDestroy(params.stream));

        // Release ALL others
        destroy_decoupled_api_handles(params);

        CHECK_NVJPEG(nvjpegJpegStateDestroy(params.nvjpeg_state));
        CHECK_NVJPEG(nvjpegDestroy(params.nvjpeg_handle));
    }

    void init_nvjpeg() {
        nvjpegDevAllocator_t dev_allocator = {&dev_malloc, &dev_free};
        nvjpegPinnedAllocator_t pinned_allocator = {&host_malloc, &host_free};

        // Try to create with Hardware Supported
        nvjpegStatus_t status = nvjpegCreateEx(NVJPEG_BACKEND_HARDWARE, &dev_allocator, &pinned_allocator,
                                               NVJPEG_FLAGS_DEFAULT, &params.nvjpeg_handle);
        params.hw_decode_available = true;
        if (status == NVJPEG_STATUS_ARCH_MISMATCH) {
            spdlog::warn("Hardware Decoder not supported. Falling back to default backend");
            // Fallback to default
            CHECK_NVJPEG(nvjpegCreateEx(NVJPEG_BACKEND_DEFAULT, &dev_allocator, &pinned_allocator, NVJPEG_FLAGS_DEFAULT,
                                        &params.nvjpeg_handle));
            params.hw_decode_available = false;
        } else {
            CHECK_NVJPEG(status);
        }
        CHECK_NVJPEG(nvjpegJpegStateCreate(params.nvjpeg_handle, &params.nvjpeg_state));

        // ???
        create_decoupled_api_handles(params);

        // stream for decoding
        CHECK_CUDA(cudaStreamCreateWithFlags(&params.stream, cudaStreamNonBlocking));
    }

    struct AsyncDecCtx {
        std::vector<nvjpegImage_t> output_jpeg_buffers;
        std::function<void(bool, const std::string &)> cb;
        void batch_finish() {}
    };

public:
    /**
     * @brief But still.....................DO NOT USE IT
     * 
     * @param file_data 
     * @param cb 
     */
    void process_images_async(const FileData &file_data, const std::function<void(bool, const std::string &)> &cb) {

        auto time_0 = std::chrono::high_resolution_clock::now();
        // Setup Batch Size( by cc )
        params.batch_size = file_data.size();

        std::vector<int> widths(params.batch_size);
        std::vector<int> heights(params.batch_size);
        std::vector<size_t> file_len(params.batch_size);

        // output buffers
        std::vector<nvjpegImage_t> iout(params.batch_size);
        // output buffer sizes, for convenience
        std::vector<nvjpegImage_t> isz(params.batch_size);

        // clear-set output buffers
        for (int i = 0; i < iout.size(); i++) {
            for (int c = 0; c < NVJPEG_MAX_COMPONENT; c++) {
                iout[i].channel[c] = NULL;
                iout[i].pitch[c] = 0;
                isz[i].pitch[c] = 0;
            }
        }

        // 1. Prepare Buffers
        if (prepare_buffers(file_data, file_len, widths, heights, iout, isz, params)) {
            release_buffers(iout);

            // Callback for failure
            if (cb) { cb(false, ""); }
            return;
        }

        // 2. Decode Batch Buffers
        if (decode_images_core(file_data, file_len, iout, params, cu_time_helper)) {
            // Push Host Function Callback
            auto ctx = new AsyncDecCtx;
            ctx->cb = cb;
            ctx->output_jpeg_buffers = iout;
            cudaLaunchHostFunc(
                params.stream,
                [](void *p) {
                    auto c = (AsyncDecCtx *)p;
                    c->batch_finish();
                    spdlog::info("Batch finish: {}", c->output_jpeg_buffers.size());
                    release_buffers(c->output_jpeg_buffers);
                    delete c;
                },
                ctx);
        }

        auto time_1 = std::chrono::high_resolution_clock::now();
        auto time_d = std::chrono::duration<double>(time_1 - time_0).count();

        last_decode_gpu_time = 0;
        last_decode_cpu_time = time_d;
    }

    bool process_images(const FileData &file_data) {
        auto time_0 = std::chrono::high_resolution_clock::now();
        // Setup Batch Size( by cc )
        params.batch_size = file_data.size();

        std::vector<int> widths(params.batch_size);
        std::vector<int> heights(params.batch_size);
        std::vector<size_t> file_len(params.batch_size);

        // output buffers
        std::vector<nvjpegImage_t> iout(params.batch_size);
        // output buffer sizes, for convenience
        std::vector<nvjpegImage_t> isz(params.batch_size);

        // clear-set output buffers
        for (int i = 0; i < iout.size(); i++) {
            for (int c = 0; c < NVJPEG_MAX_COMPONENT; c++) {
                iout[i].channel[c] = NULL;
                iout[i].pitch[c] = 0;
                isz[i].pitch[c] = 0;
            }
        }

        // 1. Read Batch
        /**
         * 
         */

        // 2. Prepare Buffers
        if (prepare_buffers(file_data, file_len, widths, heights, iout, isz, params)) {
            release_buffers(iout);
            return false;
        }

        // 3. Decode Batch Buffers
        double total;
        if (decode_images(file_data, file_len, iout, params, total, cu_time_helper)) {}
        // Finally release buffer;
        release_buffers(iout);

        auto time_1 = std::chrono::high_resolution_clock::now();
        auto time_d = std::chrono::duration<double>(time_1 - time_0).count();

        last_decode_gpu_time = total;
        last_decode_cpu_time = time_d;
        return true;

        // std::cout << "Total decoding time: " << total << " (s), " << params.batch_size << std::endl;
        // std::cout << "Avg decoding time per image: " << total / params.batch_size << " (s)" << std::endl;
        // std::cout << "Frame Per Second: " << 1 / total * params.batch_size << " (fps)" << std::endl;
        // spdlog::info("CPU time: {:.3f}, FPS in cpu: {:.1f}", time_d, 1 / time_d * params.batch_size);
    }
    double last_decode_gpu_time = 0;
    double last_decode_cpu_time = 0;
    decode_params_t params;
    CuEventHelper cu_time_helper;
};

inline bool is_interleaved(nvjpegOutputFormat_t format) {
    if (format == NVJPEG_OUTPUT_RGBI || format == NVJPEG_OUTPUT_BGRI) {
        return true;
    } else {
        return false;
    }
}

inline int prepare_buffer(const unsigned char *image_data, size_t image_len, nvjpegHandle_t handle,
                          nvjpegOutputFormat_t oformat, nvjpegImage_t &image_desc) {
    int widths[NVJPEG_MAX_COMPONENT];
    int heights[NVJPEG_MAX_COMPONENT];
    int channels;
    size_t pitchDesc;
    nvjpegChromaSubsampling_t subsampling;

    CHECK_NVJPEG(nvjpegGetImageInfo(handle, image_data, image_len, &channels, &subsampling, widths, heights));

    if (subsampling == NVJPEG_CSS_UNKNOWN) { return EXIT_FAILURE; }

    ///////////////////////////////////////////////////////////////////
    // Alloc Buffer
    ///////////////////////////////////////////////////////////////////

    // https://github.com/NVIDIA/CUDALibrarySamples/blob/eb89d592ee/nvJPEG/Image-Resize/imageResize.cpp#L123
    if (is_interleaved(oformat)) {
        pitchDesc = NVJPEG_MAX_COMPONENT * widths[0];
    } else {
        pitchDesc = 3 * widths[0];
    }

    // alloc output buffer memory(device)
    unsigned char *pBuffer = nullptr;
    cudaError_t eCopy = cudaMalloc((void **)&pBuffer, pitchDesc * heights[0]);
    if (cudaSuccess != eCopy) {
        std::cerr << "cudaMalloc failed : " << cudaGetErrorString(eCopy) << std::endl;
        return EXIT_FAILURE;
    }

    // fill output info
    image_desc.channel[0] = pBuffer;
    image_desc.channel[1] = pBuffer + widths[0] * heights[0];
    image_desc.channel[2] = pBuffer + widths[0] * heights[0] * 2;
    image_desc.pitch[0] = (unsigned int)(is_interleaved(oformat) ? widths[0] * NVJPEG_MAX_COMPONENT : widths[0]);
    image_desc.pitch[1] = (unsigned int)widths[0];
    image_desc.pitch[2] = (unsigned int)widths[0];

    if (is_interleaved(oformat)) {
        image_desc.channel[3] = pBuffer + widths[0] * heights[0] * 3;
        image_desc.pitch[3] = (unsigned int)widths[0];
    }

    return EXIT_SUCCESS;
}

class NvJpegDeocderSimple {
private:
    nvjpegHandle_t nvjpeg_handle_ = nullptr;
    nvjpegJpegState_t nvjpeg_state_ = nullptr;
    cudaStream_t stream_ = nullptr;
    CuEventHelper helper_;

public:
    NvJpegDeocderSimple() {
        // Pre-request, CUDA stream
        CHECK_CUDA(cudaStreamCreateWithFlags(&stream_, cudaStreamNonBlocking));

        // https://docs.nvidia.com/cuda/nvjpeg/index.html#nvjpeg-single-image-decoding

        // 1. Create nvJPEG library handle with one of the helper functions nvjpegCreateSimple() or nvjpegCreateEx().
        CHECK_NVJPEG(nvjpegCreateSimple(&nvjpeg_handle_));

        // 2. Create JPEG state with the helper function nvjpegJpegStateCreate(). See nvJPEG Type Declarations and nvjpegJpegStateCreate().
        CHECK_NVJPEG(nvjpegJpegStateCreate(nvjpeg_handle_, &nvjpeg_state_));
    }

    ~NvJpegDeocderSimple() {
        CHECK_NVJPEG(nvjpegJpegStateDestroy(nvjpeg_state_));
        CHECK_NVJPEG(nvjpegDestroy(nvjpeg_handle_));

        CHECK_CUDA(cudaStreamDestroy(stream_));
    }

    bool test_process(const std::shared_ptr<std::vector<char>> &file_data) {
        nvjpegImage_t iout;
        nvjpegOutputFormat_t ofmt = NVJPEG_OUTPUT_BGR;
        auto data_p = (const unsigned char *)file_data->data();
        auto size_n = file_data->size();
        if (prepare_buffer(data_p, size_n, nvjpeg_handle_, ofmt, iout) == 0) {
            helper_.recordStart(stream_);
            CHECK_NVJPEG(nvjpegDecode(nvjpeg_handle_, nvjpeg_state_, data_p, size_n, ofmt, &iout, stream_));
            helper_.recordStop(stream_);

            double decode_gpu_time;
            helper_.synchronize(decode_gpu_time);
            // spdlog::info("Decode Used: {:.3f}, {:.1f} fps", decode_gpu_time, 1.0 / decode_gpu_time);

            CHECK_CUDA(cudaFree(iout.channel[0]));

            return true;
        }
        return false;
    }
};

}// namespace gddi::experiments