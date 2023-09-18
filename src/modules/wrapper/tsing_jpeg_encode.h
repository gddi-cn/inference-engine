/*
 * Copyright (C) Beijing Tsing Micro Co.,Ltd. All rights reserved.
 * Description:
 * Author: Tsing Micro solution-app group
 * Create: 2023/04/12
 */
#ifndef __JPEG_ENCODE_H__
#define __JPEG_ENCODE_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {

#include <semaphore.h>
#include <libavutil/frame.h>
#include "mpi_vdec.h"

#endif
#endif /* __cplusplus */

#define ALIGN32(_x) (((_x) + 0x1f) & ~0x1f)
#define JENC_MAX_NUM 8
#define USE_JPEG_SEND_COPY 0
#define JPEG_MAX_CHN_NUM 8
#define JPEG_SEND_THREAD 0
#define AV_FRAME 1

#define JPEG_PRT(fmt...)                                                                                               \
    do {                                                                                                               \
        printf("[%s]-%d: ", __func__, __LINE__);                                                                       \
        printf(fmt);                                                                                                   \
    } while (0)

#define CHECK_NULL_PTR(ptr)                                                                                            \
    do {                                                                                                               \
        if (NULL == ptr) {                                                                                             \
            printf("func:%s,line:%d, NULL pointer\n", __func__, __LINE__);                                             \
            return TS_FAILURE;                                                                                         \
        }                                                                                                              \
    } while (0)

enum JPEG_CONSUME_TYPE { JPEG_FILE = 0, JPEG_COPY, JPEG_BOTH, JPEG_ZERO_COPY };

/*******************************************************
 *   structure define
 *******************************************************/
typedef struct tsJPEG_GETSTREAM_PARA_S {
    TS_BOOL bThreadStart;
    VENC_CHN VeChn[VENC_MAX_CHN_NUM];
    TS_S32 s32Cnt;
    enum JPEG_CONSUME_TYPE consume_type;
    sem_t stream_sem;

} JPEG_GETSTREAM_PARA_S;
typedef struct tsJPEG_CHN_CONFIG_S {
    PAYLOAD_TYPE_E enType;
    SIZE_S stSize;
    TS_U32 fixQp;
    TS_U32 u32Gop;
    TS_U32 u32FrameRate;
    TS_U32 u32BitRate;
    TS_U32 u32Profile;
    TS_BOOL bRcnRefShareBuf;//borrowing for pixel format
} JPEG_CHN_CONFIG_S;
struct JPEG_USER_CUSTOMIZE {
    TS_BOOL enable;

    int buf_size_rate;
    struct tsJPEG_CHN_CONFIG_S venc_chn_cfg;
};
typedef struct {
    unsigned char is_frame_enable_run;
    VENC_CHN VencChn;
    char fileName[128];
    TS_S32 fileLen;
    SIZE_S frmSize;
    VIDEO_FRAME_INFO_S *frame_info;
    sem_t frame_sem;
} _sample_jpeg_readYuv_thrdParam_s;
typedef struct tsJPEG_ENCODE_CONTEXT_S {
    // unsigned char is_stream_enable_run;
    pthread_t stream_pid;
    JPEG_GETSTREAM_PARA_S stream_thread_para;
    _sample_jpeg_readYuv_thrdParam_s frame_thread_para;
    pthread_t frame_pid;
    int index;
    unsigned int pic_w;
    unsigned int pic_h;
    const char *outfilename;
    unsigned char pic_save_path[64];
    unsigned int chn;
    unsigned int jpeg_init_done;

} JPEG_ENCODE_CONTEXT_S;
typedef struct tsJPEG_CONFIG_S {
    unsigned int image_width;
    unsigned int image_height;
    unsigned int jpeg_chn;
    enum JPEG_CONSUME_TYPE consume_type;
} JPEG_CONFIG_S;

typedef struct tsJPEG_CONFIG_MULTI_S {
    JPEG_CONFIG_S jpeg_config[JENC_MAX_NUM];
    int jenc_total_num;
} JPEG_CONFIG_MULTI_S;

typedef struct tsJPEG_COMPRESS_S {

    unsigned int jpeg_chn;
    // enum JPEG_CONSUME_TYPE consume_type;
    unsigned char *in_image_buffer[3];
    unsigned char *out_image_buffer;
    unsigned int out_image_size;
#if AV_FRAME
    AVFrame *frame_in;
#else
    VIDEO_FRAME_INFO_S *frame_in;
#endif
    unsigned char jpeg_file_name[50];
    unsigned char jpeg_file_save_path[100];
} JPEG_COMPRESS_S;

#ifndef __KERNEL__
#include <sys/time.h>
#endif

int jpeg_encode_init(JPEG_CONFIG_S *init_para);
int jpeg_encode_deinit(int jenc_chn);
int jpeg_encode_convert(JPEG_COMPRESS_S *jpeg_data);

void set_jpeg_save_path(unsigned char *path);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace codec {

class TsingJpegEncode {
public:
    TsingJpegEncode();
    ~TsingJpegEncode();

    bool init_codecer(const uint32_t width, const uint32_t height);
    uint32_t codec_image(const std::shared_ptr<AVFrame> &frame, const uint8_t *jpeg_data);
    bool save_image(const std::shared_ptr<AVFrame> &frame, const std::string &path);

private:
    JPEG_CONFIG_S init_para_;
};

}// namespace codec

#endif