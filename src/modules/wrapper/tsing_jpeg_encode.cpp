/*
 * Copyright (C) Beijing Tsing Micro Co.,Ltd. All rights reserved.
 * Description:
 * Author: Tsing Micro solution-app group
 * Create: 2023/04/09
 */

#include "cJSON.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

extern "C" {
// #include "sample_comm.h"
// #include "rtsp_client_api.h"
#include "libavcodec/avcodec.h"

#include "mpi_sys.h"
#include "mpi_vb.h"
#include "mpi_vdec.h"
#include "mpi_venc.h"
#include "semaphore.h"
#include "ts_buffer.h"
#include "ts_comm_sys.h"
#include "ts_comm_vb.h"
#include "ts_comm_vdec.h"
#include "ts_comm_venc.h"
#include "ts_common.h"
#include "ts_defines.h"
#include "ts_math.h"
#include "ts_type.h"
#include <sys/time.h>
// #include "sample_edge.h"
#include <sys/prctl.h>
// #include "algo_comm.h"
}

#include "tsing_jpeg_encode.h"

static unsigned char jpeg_chn_exit[JENC_MAX_NUM] = {0};
static int jenc_total_num = 0;
static TS_U8 g_jenc_save_path[128] = {0};
static JPEG_ENCODE_CONTEXT_S stJpegEncodeCtxArr[JENC_MAX_NUM] = {0};
static unsigned char *s_pImageOutBuffer[JENC_MAX_NUM] = {NULL};
static unsigned int *s_pImageOutBufferSize[JENC_MAX_NUM] = {NULL};
static unsigned char s_pImageFileName[JENC_MAX_NUM][100] = {0};

int creat_jpeg_send_frame_thread(JPEG_ENCODE_CONTEXT_S *pstJpegEncodeCtxArr);
int jpeg_jenc_start(VENC_CHN VencChn, JPEG_CHN_CONFIG_S *pChnCfg);
int creat_jpeg_get_stream_thread(JPEG_ENCODE_CONTEXT_S *pstJpegEncodeCtxArr);
int destory_jpeg_get_stream_thread(JPEG_ENCODE_CONTEXT_S *pstJpegEncodeCtxArr);
int destory_jpeg_send_frame_thread(JPEG_ENCODE_CONTEXT_S *pstJpegEncodeCtxArr);
int jpeg_stop(VENC_CHN VencChn);

static TS_U32 jpeg_get_system_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static TS_S32 JPEG_SYS_Init(VB_CONFIG_S *pstVbConfig) {
    TS_S32 s32Ret = TS_FAILURE;

    TS_MPI_SYS_Exit();
    TS_MPI_VB_Exit();

    if (NULL == pstVbConfig) {
        JPEG_PRT("input parameter is null, it is invaild!\n");
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_VB_SetConfig(pstVbConfig);

    if (TS_SUCCESS != s32Ret) {
        JPEG_PRT("TS_MPI_VB_SetConf failed!\n");
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_VB_Init();

    if (TS_SUCCESS != s32Ret) {
        JPEG_PRT("TS_MPI_VB_Init failed!\n");
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_SYS_Init();
    if (TS_SUCCESS != s32Ret) {
        JPEG_PRT("TS_MPI_SYS_Init failed!\n");
        TS_MPI_VB_Exit();
        return TS_FAILURE;
    }

    return s32Ret;
}

TS_S32 SAMPLE_JPEG_SYS_InitEx(SIZE_S *pInSize) {
    TS_S32 s32Ret;
    TS_U64 u64BlkSize;
    VB_CONFIG_S stVbConf;

    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));

    u64BlkSize = COMMON_GetPicBufferSize(pInSize->u32Width, pInSize->u32Height, PIXEL_FORMAT_YVU_PLANAR_420,
                                         DATA_BITWIDTH_8, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize = u64BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = 20;

    s32Ret = JPEG_SYS_Init(&stVbConf);
    if (TS_SUCCESS != s32Ret) {
        JPEG_PRT("JPEG_SYS_Init failed!\n");
        return s32Ret;
    }

    JPEG_PRT("JPEG_SYS_Init success!\n");

    return TS_SUCCESS;
}

/******************************************************************************
* function: raw data(YUV) encode to jpeg fun
******************************************************************************/
int jpeg_encode_init(JPEG_CONFIG_S *init_para) {
    int ret;

    SIZE_S pInSize = {0};
    int jenc_chn = 0;
    CHECK_NULL_PTR(init_para);
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&init_mutex);
    jenc_total_num++;

    // jenc_total_num = init_para->jenc_total_num;

    if (jenc_total_num > JENC_MAX_NUM) {
        printf("err: jenc chn num over max\n");
        return TS_FAILURE;
    }

    pInSize.u32Width = init_para->image_width;
    pInSize.u32Height = init_para->image_height;
    if (pInSize.u32Width > 10000 || pInSize.u32Width < 64) {
        printf("jpeg_encode_init set jpeg width para is err\n");
        return -1;
    }
    if (pInSize.u32Width > 10000 || pInSize.u32Width < 64) {
        printf("jpeg_encode_init set jpeg height para is err\n");
        return -1;
    }

    // jpeg_init_done = 0;

    int i = 0;
    i = init_para->jpeg_chn;

    jpeg_chn_exit[i] = 0;
    stJpegEncodeCtxArr[i].jpeg_init_done = 0;
#if USE_JPEG_SEND_THREAD
    stJpegEncodeCtxArr[i].frame_pid = i;
    stJpegEncodeCtxArr[i].frame_thread_para.VencChn = i;
    stJpegEncodeCtxArr[i].frame_thread_para.is_frame_enable_run = 1;
    stJpegEncodeCtxArr[i].frame_thread_para.frmSize.u32Width = pInSize.u32Width;
    stJpegEncodeCtxArr[i].frame_thread_para.frmSize.u32Height = pInSize.u32Height;
    stJpegEncodeCtxArr[i].frame_thread_para.frame_info = (VIDEO_FRAME_INFO_S *)malloc(sizeof(VIDEO_FRAME_INFO_S));
    CHECK_NULL_PTR(stJpegEncodeCtxArr[i].frame_thread_para.frame_info);
    sem_init(&stJpegEncodeCtxArr[i].frame_thread_para.frame_sem, 0, 0);
    // sem_init(&stJpegEncodeCtxArr[i].stream_thread_para.stream_sem, 0, 0);

    ret = creat_jpeg_send_frame_thread(&stJpegEncodeCtxArr[i]);
    if (TS_SUCCESS != ret) {
        printf(" creat jpeg send frame thread faild,ret:%d\n", ret);
        return TS_FAILURE;
    }
#endif

    /******************************************
    start stream jpeg
    ******************************************/

    struct JPEG_USER_CUSTOMIZE usr_custom;
    jenc_chn = init_para->jpeg_chn;
    usr_custom.venc_chn_cfg.enType = PT_JPEG;
    usr_custom.venc_chn_cfg.stSize.u32Width = pInSize.u32Width;
    usr_custom.venc_chn_cfg.stSize.u32Height = pInSize.u32Height;

    ret = jpeg_jenc_start(jenc_chn, &usr_custom.venc_chn_cfg);
    if (TS_SUCCESS != ret) {
        printf("jenc chn %d start failed for %#x!\n", i, ret);
        return TS_FAILURE;
    }

    /******************************************
     stream save process
    ******************************************/

    stJpegEncodeCtxArr[i].stream_thread_para.consume_type = init_para->consume_type;
#if 1
    if (jenc_total_num == 1) {

        stJpegEncodeCtxArr[0].stream_pid = 0;
        stJpegEncodeCtxArr[0].stream_thread_para.bThreadStart = TS_TRUE;
        stJpegEncodeCtxArr[0].stream_thread_para.s32Cnt = JENC_MAX_NUM;

        sem_init(&stJpegEncodeCtxArr[0].stream_thread_para.stream_sem, 0, 0);

        for (size_t j = 0; j < stJpegEncodeCtxArr[0].stream_thread_para.s32Cnt; j++) {
            stJpegEncodeCtxArr[0].stream_thread_para.VeChn[j] = j;
        }

        ret = creat_jpeg_get_stream_thread(&stJpegEncodeCtxArr[0]);
        if (TS_SUCCESS != ret) {
            JPEG_PRT("Start jpeg failed!ret:%d\n", ret);
            goto JPEG_STOP_EXIT;
        }
        printf("jpeg chn %d recving stream now ...\n", i);
    }
#else

    ret = destory_jpeg_get_stream_thread(&stJpegEncodeCtxArr[0]);
    if (ret) printf("destory jpeg get stream thread failed!ret:%d\n", ret);

    if (jenc_total_num == 0) {
        sem_destroy(&stJpegEncodeCtxArr[0].stream_thread_para.stream_sem);

        stJpegEncodeCtxArr[0].stream_pid = 0;
        stJpegEncodeCtxArr[0].stream_thread_para.bThreadStart = TS_TRUE;
        stJpegEncodeCtxArr[0].stream_thread_para.s32Cnt = jenc_total_num;

        sem_init(&stJpegEncodeCtxArr[0].stream_thread_para.stream_sem, 0, 0);

        for (size_t j = 0; j < stJpegEncodeCtxArr[0].stream_thread_para.s32Cnt; j++) {
            stJpegEncodeCtxArr[0].stream_thread_para.VeChn[j] = j;
        }

        ret = creat_jpeg_get_stream_thread(&stJpegEncodeCtxArr[0]);
        if (TS_SUCCESS != ret) {
            JPEG_PRT("Start jpeg failed!ret:%d\n", ret);
            goto JPEG_STOP_EXIT;
        }
        printf("jpeg chn %d recving stream now ...\n", i);

#endif
    usleep(100000);
    stJpegEncodeCtxArr[i].jpeg_init_done = 1;
    pthread_mutex_unlock(&init_mutex);
    printf("jpeg chn %d init done \n", i);

    return ret;

JPEG_STOP_EXIT:
    printf("jpeg init exit abnormally now ...\n");

    ret = jpeg_stop(i);
    if (ret) printf("jpeg chn %d stop failed!ret:%d\n", i, ret);
    ret = destory_jpeg_send_frame_thread(&stJpegEncodeCtxArr[i]);
    if (ret) printf("jpeg chn %d destory send frame thread failed!ret:%d\n", i, ret);
    sem_destroy(&stJpegEncodeCtxArr[i].frame_thread_para.frame_sem);
    // sem_destroy(&stJpegEncodeCtxArr[i].stream_thread_para.stream_sem);

    // sem_destroy(&stJpegEncodeCtxArr[0].stream_thread_para.stream_sem);

    return ret;
}

int jpeg_encode_deinit(int jenc_chn) {
    int ret = 0;
    int i = 0;

    static pthread_mutex_t deinit_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&deinit_mutex);
    i = jenc_chn;

    if (jenc_total_num) jenc_total_num--;

    if (jenc_total_num == 0) {
        ret = destory_jpeg_get_stream_thread(&stJpegEncodeCtxArr[0]);
        if (ret) printf("destory jpeg get stream thread failed!ret:%d\n", ret);
        printf("destory jpeg get stream thread ok!\n");
    }

    jpeg_chn_exit[i] = 1;
    // usleep(800000);
    ret = jpeg_stop(i);
    if (ret) printf("jpeg chn %d stop failed!ret:%d\n", i, ret);
    // usleep(2000000);

    pthread_mutex_unlock(&deinit_mutex);
#if USE_JPEG_SEND_THREAD
    ret = destory_jpeg_send_frame_thread(&stJpegEncodeCtxArr[i]);
    if (ret) printf("jpeg chn %d destory send frame thread failed!ret:%d\n", i, ret);
    sem_destroy(&stJpegEncodeCtxArr[i].frame_thread_para.frame_sem);
#endif
    // sem_destroy(&stJpegEncodeCtxArr[i].stream_thread_para.stream_sem);

    if (jenc_total_num == 0) {
        sem_destroy(&stJpegEncodeCtxArr[0].stream_thread_para.stream_sem);

        // ret = destory_jpeg_get_stream_thread(&stJpegEncodeCtxArr[0]);
        // if (ret)
        // 	printf("destory jpeg get stream thread failed!ret:%d\n",ret);
    }
    jpeg_chn_exit[i] = 2;

    return ret;
}

// int jpeg_code_convert(int jenc_chn, unsigned char *image_in, unsigned char *image_out, unsigned char *image_out_size)
int jpeg_encode_convert(JPEG_COMPRESS_S *jpeg_data) {
    int i = 0;
    int s32Ret = 0;
    unsigned char *image_out = NULL;
    unsigned char *image_out_size = NULL;

    image_out = jpeg_data->out_image_buffer;
    image_out_size = (unsigned char *)&jpeg_data->out_image_size;

#if USE_JPEG_SEND_COPY
    unsigned char *image_in[3] = {NULL};
    image_in[0] = jpeg_data->in_image_buffer[0];
    // image_in[1] = jpeg_data->in_image_buffer[1];
    CHECK_NULL_PTR(image_in[0]);
#endif

    i = jpeg_data->jpeg_chn;
    if (jpeg_data->jpeg_chn > JENC_MAX_NUM) {
        printf("err!!!:jpeg chn is over max num\n");
        return -1;
    }

    if (stJpegEncodeCtxArr[i].stream_thread_para.consume_type == JPEG_COPY) {
        CHECK_NULL_PTR(image_out);
        CHECK_NULL_PTR(image_out_size);
        s_pImageOutBuffer[i] = image_out;
        s_pImageOutBufferSize[i] = (unsigned int *)image_out_size;
    } else if (stJpegEncodeCtxArr[i].stream_thread_para.consume_type == JPEG_FILE) {

        strncpy((char *)&s_pImageFileName[i][0], (char *)jpeg_data->jpeg_file_save_path, 256);
        // set_pic_save_path(jpeg_data->jpeg_file_save_path);
    }

#if USE_JPEG_SEND_THREAD
#if USE_JPEG_SEND_COPY
    stJpegEncodeCtxArr[i].frame_thread_para.frame_info->stVFrame.u64VirAddr[0] = (TS_U64)image_in[0];
    // stJpegEncodeCtxArr[i].frame_thread_para.frame_info->stVFrame.u64VirAddr[1] = (TS_U64)image_in[1];
#else
#if AV_FRAME
    memcpy(stJpegEncodeCtxArr[i].frame_thread_para.frame_info, (VIDEO_FRAME_INFO_S *)jpeg_data->frame_in->buf[0]->data,
           sizeof(VIDEO_FRAME_INFO_S));
#else
    memcpy(stJpegEncodeCtxArr[i].frame_thread_para.frame_info, (VIDEO_FRAME_INFO_S *)jpeg_data->frame_in,
           sizeof(VIDEO_FRAME_INFO_S));
#endif
#endif
    sem_post(&stJpegEncodeCtxArr[i].frame_thread_para.frame_sem);
#else
        if (!stJpegEncodeCtxArr[i].jpeg_init_done) {

            JPEG_PRT("jpeg chn%d init not done! \n", i);
            return -1;
        }
        if (jpeg_chn_exit[i]) {

            JPEG_PRT("jpeg chn%d exit! \n", i);
            return -1;
        }
#if AV_FRAME

        s32Ret = TS_MPI_VENC_SendFrame(i, (USER_FRAME_INFO_S *)jpeg_data->frame_in->buf[0]->data, -1);
        if (TS_SUCCESS != s32Ret) { JPEG_PRT("TS_MPI_VENC_SendFrame Err s32Ret:%d\n", s32Ret); }
#else
        s32Ret = TS_MPI_VENC_SendFrame(i, (USER_FRAME_INFO_S *)jpeg_data->frame_in, -1);
        if (TS_SUCCESS != s32Ret) { JPEG_PRT("TS_MPI_VENC_SendFrame Err s32Ret:%d\n", s32Ret); }
#endif

#endif
    if (stJpegEncodeCtxArr[0].stream_thread_para.bThreadStart == TS_TRUE) {
        sem_wait(&stJpegEncodeCtxArr[0].stream_thread_para.stream_sem);
    } else {
        JPEG_PRT("jpeg chn %d get stream stop\n", i);
        return -1;
    }

    return 0;
}

TS_S32 jpeg_init_frame(VIDEO_FRAME_INFO_S *pFrame, VB_BLK VbBlk, SIZE_S *pAlign, VB_POOL pool) {
    void *viraddr = NULL;
    TS_U64 phyaddr = TS_MPI_VB_Handle2PhysAddr(VbBlk);

    TS_MPI_VB_GetBlockVirAddr(pool, phyaddr, &viraddr);

    JPEG_PRT("pool_%d phyaddr_%lld viraddr_%p\n", pool, phyaddr, viraddr);

    pFrame->u32PoolId = TS_MPI_VB_Handle2PoolId(VbBlk);
    pFrame->stVFrame.enPixelFormat = PIXEL_FORMAT_NV_12;
    pFrame->stVFrame.enField = VIDEO_FIELD_FRAME;
    pFrame->stVFrame.enCompressMode = COMPRESS_MODE_NONE;
    pFrame->stVFrame.u64PhyAddr[0] = phyaddr;
    pFrame->stVFrame.u64PhyAddr[1] = pFrame->stVFrame.u64PhyAddr[0] + pAlign->u32Width * pAlign->u32Height;
    pFrame->stVFrame.u64PhyAddr[2] = pFrame->stVFrame.u64PhyAddr[1] + pAlign->u32Width * pAlign->u32Height / 2;

    pFrame->stVFrame.u64VirAddr[0] = (TS_U64)viraddr;
    pFrame->stVFrame.u64VirAddr[1] = pFrame->stVFrame.u64VirAddr[0] + pAlign->u32Width * pAlign->u32Height;
    pFrame->stVFrame.u64VirAddr[2] = pFrame->stVFrame.u64VirAddr[0] + pAlign->u32Width * pAlign->u32Height * 3 / 2;

    pFrame->stVFrame.u32Width = pAlign->u32Width;
    pFrame->stVFrame.u32Height = pAlign->u32Height;
    pFrame->stVFrame.u32Stride[0] = pAlign->u32Width;
    pFrame->stVFrame.u32Stride[1] = pAlign->u32Width;
    pFrame->stVFrame.u32Stride[2] = pAlign->u32Width;
    pFrame->stVFrame.u64PTS = jpeg_get_system_time_ms();

    return TS_SUCCESS;
}

TS_VOID *jpeg_send_frame_proc(TS_VOID *param) {
    _sample_jpeg_readYuv_thrdParam_s *pThrdParam = (_sample_jpeg_readYuv_thrdParam_s *)param;

    TS_S32 s32Ret = 0;

    JPEG_PRT("param=%p. vencChn[%d] WH=[%d,%d], file[%s]\n", pThrdParam, pThrdParam->VencChn,
             pThrdParam->frmSize.u32Width, pThrdParam->frmSize.u32Height, pThrdParam->fileName);
#if USE_JPEG_SEND_COPY
    SIZE_S size_align = {0};
    TS_S32 sendCnt = 0;
    TS_S32 frmSize, totalCnt = 0;
    VB_POOL hPool = VB_INVALID_POOLID;
    TS_U64 u64BlkSize;
    VB_POOL_CONFIG_S stVbPoolCfg = {0};
    u64BlkSize =
        COMMON_GetPicBufferSize(pThrdParam->frmSize.u32Width, pThrdParam->frmSize.u32Height,
                                PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, DEFAULT_ALIGN);

    frmSize = pThrdParam->frmSize.u32Width * pThrdParam->frmSize.u32Height * 3 / 2;

    JPEG_PRT("u64BlkSize:%lld, frmSize=%d,totalCnt:%d\n", u64BlkSize, frmSize, totalCnt);

    stVbPoolCfg.u64BlkSize = u64BlkSize;
    stVbPoolCfg.u32BlkCnt = 10;//frmCnt;

    hPool = TS_MPI_VB_CreatePool(&stVbPoolCfg);
    if (hPool == VB_INVALID_POOLID) {
        JPEG_PRT("TS_MPI_VB_CreatePool failed, blkSize=%lld, count=%d\n", stVbPoolCfg.u64BlkSize,
                 stVbPoolCfg.u32BlkCnt);
        goto creat_vb_faild;
    }
    JPEG_PRT("TS_MPI_VB_CreatePool success, blkSize=%lld, u32BlkCnt=%d. filefrmCnt=%d\n", u64BlkSize,
             stVbPoolCfg.u32BlkCnt, totalCnt);

    TS_MPI_VB_MmapPool(hPool);

    size_align.u32Width = ALIGN_UP(pThrdParam->frmSize.u32Width, 8);
    size_align.u32Height = ALIGN_UP(pThrdParam->frmSize.u32Height, 2);
    long t0, t1;
#endif
    while (pThrdParam->is_frame_enable_run) {

        if (stJpegEncodeCtxArr[pThrdParam->VencChn].jpeg_init_done == TS_FALSE) {
            usleep(100 * 1000);
            continue;
        }

        sem_wait(&pThrdParam->frame_sem);

#if USE_JPEG_SEND_COPY
        VIDEO_FRAME_INFO_S vFrmInfo = {0};
        VB_BLK VbBlk = VB_INVALID_HANDLE;
        VbBlk = TS_MPI_VB_GetBlock(hPool, u64BlkSize, NULL);
        if (VB_INVALID_HANDLE == VbBlk) {
            JPEG_PRT("TS_MPI_VB_GetBlock err! size:%lld\n", u64BlkSize);
            break;
        }

        jpeg_init_frame(&vFrmInfo, VbBlk, &size_align, hPool);

        vFrmInfo.stVFrame.u32TimeRef = sendCnt * 2;

#endif

        static ts_s32 print_count = 0;
        if (print_count % 100 == 0) JPEG_PRT("BF send frame\n");
        print_count++;

#if USE_JPEG_SEND_COPY
        // t0 = get_system_time_us();
        memcpy((TS_U8 *)vFrmInfo.stVFrame.u64VirAddr[0], (TS_U8 *)pThrdParam->frame_info->stVFrame.u64VirAddr[0],
               pThrdParam->frmSize.u32Width * pThrdParam->frmSize.u32Height * 3 / 2);
        // memcpy((TS_U8*)vFrmInfo.stVFrame.u64VirAddr[1], (TS_U8*)pThrdParam->frame_info->stVFrame.u64VirAddr[1],
        // pThrdParam->frmSize.u32Width * pThrdParam->frmSize.u32Height >>1);
        t1 = get_system_time_us();
        // printf("xxxxxxxxxxxxxxxxxxjpeg memcpytime:[%ld]\n", t1 - t0);
        s32Ret = TS_MPI_VENC_SendFrame(pThrdParam->VencChn, &vFrmInfo, -1);
#else

            s32Ret = TS_MPI_VENC_SendFrame(pThrdParam->VencChn, (USER_FRAME_INFO_S *)pThrdParam->frame_info, -1);
#endif
        if (TS_SUCCESS != s32Ret) { JPEG_PRT("s32Ret:%d\n", s32Ret); }

#if USE_JPEG_SEND_COPY
        TS_MPI_VB_ReleaseBlock(VbBlk);
#endif
    }

    // creat_vb_faild:

    return NULL;
}

int creat_jpeg_send_frame_thread(JPEG_ENCODE_CONTEXT_S *pstJpegEncodeCtxArr) {
    CHECK_NULL_PTR(pstJpegEncodeCtxArr);

    pthread_create(&pstJpegEncodeCtxArr->frame_pid, 0, jpeg_send_frame_proc,
                   (void *)&pstJpegEncodeCtxArr->frame_thread_para);

    return TS_SUCCESS;
}

#if 1
#ifdef HIDE_PRINT_0919
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr)[0])//without check __must_be_array(arr)
#endif

#define UPDATE_COUNT_INTERVAL (30 * 10)
#define REFRESH_TARGET_RATE (4.0f)//per second
#define DIFF_RATE (0.5f)

static char angle_loop[] = {'|', '/', '-', '\\', '|', '/', '-', '\\'};

void update_refresh_interval(int *interval) {
    static unsigned int count = 0;
    static time_t last = 0;
    static time_t now;
    float real_call_rate;

    if (0 == count % UPDATE_COUNT_INTERVAL) {
        now = time(NULL);
        real_call_rate = ((float)UPDATE_COUNT_INTERVAL) / ((float)(now - last));

        (*interval) = (int)(real_call_rate / REFRESH_TARGET_RATE);

        *interval = (*interval <= 0) ? 1 : (*interval);

        //		printf("now=%ld, last=%ld, real_call_rate=%0.2f, interval=%d\n", now, last, real_call_rate,*interval);

        last = now;
    }
    count++;

    return;
}

void show_running_fan(void) {
    static int REFRESH_INTERVAL = 1;
    static int tick = 0;
    static int idx = 0;
    if (0 == tick % REFRESH_INTERVAL) {
        printf("\r Running <%c>", angle_loop[idx]);
        fflush(stdout);
        idx++;
        idx = (idx >= ARRAY_SIZE(angle_loop)) ? 0 : idx;
    }
    tick++;
    update_refresh_interval(&REFRESH_INTERVAL);
    return;
}
#endif

static int set_chn_attr(VENC_CHN_ATTR_S *attr, int picWidth, int picHeight) {
    JPEG_PRT("%s called!", __FUNCTION__);

    //	attr->stVencAttr.enType = _PT_JPEG;
    attr->stVencAttr.u32PicWidth = picWidth;
    attr->stVencAttr.u32PicHeight = picHeight;
    attr->stVencAttr.u32MaxPicWidth = picWidth;  // refer to hisi. it must be aligned by MIN_ALIGN(2)
    attr->stVencAttr.u32MaxPicHeight = picHeight;// refer to hisi. it must be aligned by MIN_ALIGN(2)

    attr->stVencAttr.u32Profile = 2;              // avc unsupport, decided by internal encode
    attr->stVencAttr.u32BufSize = 3 * 1024 * 1024;// 1920 * 1080 * 1.5; // buf size ##@@
    attr->stVencAttr.bByFrame = TS_TRUE;          // get stream by freame

    // 是否使能 DCF（Design rule for Camera File system）。
    // DCF 信息包含拍照基本信息和缩略图。
    attr->stVencAttr.stAttrJpege.bSupportDCF = TS_TRUE;
    // VENC_PIC_RECEIVE_MULTI 允许从多个源接收，但图像宽高需要一样
    attr->stVencAttr.stAttrJpege.enReceiveMode = VENC_PIC_RECEIVE_SINGLE;
    attr->stVencAttr.stAttrJpege.stMPFCfg.u8LargeThumbNailNum = 0;// [0,2], 0 代表不编码
    // [0,2], 0 代表不编码
    // 范围宽为(160，PicWidth) 高为(120，PicHeight) 按4对齐
    attr->stVencAttr.stAttrJpege.stMPFCfg.astLargeThumbNailSize[0].u32Width = 10;
    attr->stVencAttr.stAttrJpege.stMPFCfg.astLargeThumbNailSize[0].u32Height = 10;
    attr->stVencAttr.stAttrJpege.stMPFCfg.astLargeThumbNailSize[1].u32Width = 12;
    attr->stVencAttr.stAttrJpege.stMPFCfg.astLargeThumbNailSize[1].u32Height = 12;
    attr->stRcAttr.stMjpegFixQp.u32Qfactor = 100;

    JPEG_PRT("%s exit!", __FUNCTION__);
    return 0;
}

static TS_S32 JPEG_CreatEx(VENC_CHN VencChn, JPEG_CHN_CONFIG_S *pChnCfg) {
    TS_S32 s32Ret;
    SIZE_S stPicSize;
    VENC_CHN_ATTR_S stVencChnAttr;
    VENC_ATTR_JPEG_S stJpegAttr;

    stPicSize.u32Width = pChnCfg->stSize.u32Width;
    stPicSize.u32Height = pChnCfg->stSize.u32Height;

    /******************************************
     step 1:  Create jpeg Channel
    ******************************************/
    stVencChnAttr.stVencAttr.enType = pChnCfg->enType;
    stVencChnAttr.stVencAttr.u32MaxPicWidth = stPicSize.u32Width;
    stVencChnAttr.stVencAttr.u32MaxPicHeight = stPicSize.u32Height;
    stVencChnAttr.stVencAttr.u32PicWidth = stPicSize.u32Width;   /*the picture width*/
    stVencChnAttr.stVencAttr.u32PicHeight = stPicSize.u32Height; /*the picture height*/

    switch (pChnCfg->enType) {

        case PT_JPEG:
            stJpegAttr.bSupportDCF = TS_FALSE;
            stJpegAttr.stMPFCfg.u8LargeThumbNailNum = 0;
            stJpegAttr.enReceiveMode = VENC_PIC_RECEIVE_SINGLE;
            memcpy(&stVencChnAttr.stVencAttr.stAttrJpege, &stJpegAttr, sizeof(VENC_ATTR_JPEG_S));
            break;
        default:
            JPEG_PRT("cann't support this enType (%d) in this version!\n", pChnCfg->enType);
            return TS_ERR_VENC_NOT_SUPPORT;
    }

    if (PT_JPEG == pChnCfg->enType) {
        stVencChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP;
        stVencChnAttr.stGopAttr.stNormalP.s32IPQpDelta = 0;
    }

    if (stVencChnAttr.stVencAttr.enType == PT_JPEG) {
        set_chn_attr(&stVencChnAttr, stVencChnAttr.stVencAttr.u32PicWidth, stVencChnAttr.stVencAttr.u32PicHeight);
    }

    s32Ret = TS_MPI_VENC_CreateChn(VencChn, &stVencChnAttr);
    if (TS_SUCCESS != s32Ret) {
        JPEG_PRT("TS_MPI_VENC_CreateChn [%d] faild with %#x! ===\n", VencChn, s32Ret);
        return s32Ret;
    }

    return TS_SUCCESS;
}
#endif

/******************************************************************************
* funciton : Start jpeg stream mode
* note      : rate control parameter need adjust, according your case.
******************************************************************************/
int jpeg_jenc_start(VENC_CHN VencChn, JPEG_CHN_CONFIG_S *pChnCfg) {
    TS_S32 s32Ret;
    VENC_RECV_PIC_PARAM_S stRecvParam;

    /******************************************
     step 1:  Creat Encode Chnl
    ******************************************/
    s32Ret = JPEG_CreatEx(VencChn, pChnCfg);
    if (TS_SUCCESS != s32Ret) {
        JPEG_PRT("SAMPLE_COMM_VENC_Creat faild with%#x! \n", s32Ret);
        return TS_FAILURE;
    }

    /******************************************
     step 2:  Start Recv jpeg Pictures
    ******************************************/
    stRecvParam.s32RecvPicNum = -1;
    s32Ret = TS_MPI_VENC_StartRecvFrame(VencChn, &stRecvParam);
    if (TS_SUCCESS != s32Ret) {
        JPEG_PRT("TS_MPI_VENC_StartRecvPic faild with%#x! \n", s32Ret);
        return TS_FAILURE;
    }

    return TS_SUCCESS;
}

#if 1

/******************************************************************************
* funciton : save stream
******************************************************************************/
TS_S32 JPEG_SaveStream(FILE *pFd, VENC_STREAM_S *pstStream) {
    TS_S32 i;
    int ret;
    size_t nmemb;

    for (i = 0; i < pstStream->u32PackCount; i++) {

        nmemb = fwrite(pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset,
                       pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset, 1, pFd);
        if (nmemb != 1) JPEG_PRT("nmemb 1 should be written, but %ld actually, error %d", nmemb, errno);

        ret = fflush(pFd);
        if (ret) JPEG_PRT("fflush ret %d, error %d", ret, errno);
    }

    return TS_SUCCESS;
}
#endif

/******************************************************************************
* funciton : save stream
******************************************************************************/
TS_S32 JPEG_CopyStream(TS_U8 *pImageOut, int *pImageSize, VENC_STREAM_S *pstStream) {
    TS_S32 i;
    *pImageSize = 0;
    for (i = 0; i < pstStream->u32PackCount; i++) {
        // printf("pImageOut = %p, paddr = %p, size =%d,count =%d\n",pImageOut, pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset,
        // pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset, pstStream->u32PackCount);
        memcpy(pImageOut, pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset,
               pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset);

        *pImageSize += pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset;
    }

    return TS_SUCCESS;
}
/******************************************************************************
* funciton : get stream from each channels and save them
******************************************************************************/
TS_VOID *get_jpeg_stream_proc(TS_VOID *p) {
    TS_S32 i;
    TS_S32 s32ChnTotal;
    // VENC_CHN_ATTR_S stVencChnAttr;
    JPEG_GETSTREAM_PARA_S *pstPara;
    TS_S32 maxfd = 0;
    struct timeval TimeoutVal;
    fd_set read_fds;
    TS_U32 u32PictureCnt[JPEG_MAX_CHN_NUM] = {0};
    TS_S32 VencFd[JPEG_MAX_CHN_NUM];
    TS_CHAR aszFileName[JPEG_MAX_CHN_NUM][256];
    FILE *pFile[JPEG_MAX_CHN_NUM];
    // char szFilePostfix[10];
    VENC_CHN_STATUS_S stStat;
    VENC_STREAM_S stStream;
    TS_S32 s32Ret;
    // VENC_CHN VencChn;
    PAYLOAD_TYPE_E enPayLoadType[JPEG_MAX_CHN_NUM];
    VENC_STREAM_BUF_INFO_S stStreamBufInfo[JPEG_MAX_CHN_NUM];
    enum JPEG_CONSUME_TYPE consume_type;

    prctl(PR_SET_NAME, "GetVencStream", 0, 0, 0);

    pstPara = (JPEG_GETSTREAM_PARA_S *)p;
    if (!pstPara) return NULL;
    consume_type = pstPara->consume_type;
    s32ChnTotal = pstPara->s32Cnt;
    // long t0,t1;

    /******************************************
     step 1:  check & prepare save-file & jpeg-fd
    ******************************************/
    if (s32ChnTotal > JPEG_MAX_CHN_NUM) {
        JPEG_PRT("input count invaild\n");
        return NULL;
    }
    for (i = 0; i < s32ChnTotal; i++) {
        /* decide the stream file name, and open file to save stream */
        // VencChn = i;//pstPara->VeChn[i];
        // s32Ret = TS_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
        // if (s32Ret != TS_SUCCESS)
        // {
        //     JPEG_PRT("TS_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n",
        //                VencChn, s32Ret);
        //     return NULL;
        // }
        enPayLoadType[i] = PT_JPEG;

        // strcpy(szFilePostfix, ".jpg");

        // s32Ret = SAMPLE_COMM_VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix);
        // if (s32Ret != TS_SUCCESS)
        // {
        //     JPEG_PRT("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n",
        //                stVencChnAttr.stVencAttr.enType, s32Ret);
        //     return NULL;
        // }

        /* Set jpeg Fd. */
        VencFd[i] = TS_MPI_VENC_GetFd(i);
        if (VencFd[i] < 0) {
            JPEG_PRT("TS_MPI_VENC_GetFd failed with %#x!\n", VencFd[i]);
            return NULL;
        }
        if (maxfd <= VencFd[i]) { maxfd = VencFd[i]; }

        s32Ret = TS_MPI_VENC_GetStreamBufInfo(i, &stStreamBufInfo[i]);
        if (TS_SUCCESS != s32Ret) {
            JPEG_PRT("TS_MPI_VENC_GetStreamBufInfo failed with %#x!\n", s32Ret);
            return (void *)TS_FAILURE;
        }
    }

    /******************************************
     step 2:  Start to get streams of each channel.
    ******************************************/
    while (TS_TRUE == pstPara->bThreadStart) {
#ifdef HIDE_PRINT_0919
        show_running_fan();
#endif

        FD_ZERO(&read_fds);
        for (i = 0; i < s32ChnTotal; i++) { FD_SET(VencFd[i], &read_fds); }

        TimeoutVal.tv_sec = 15;
        TimeoutVal.tv_usec = 0;
        s32Ret = TS_MPI_VENC_SELECT(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0) {
            JPEG_PRT("select failed!\n");
            break;
        } else if (s32Ret == 0) {
            JPEG_PRT("get jpeg stream time out, try again\n");
            if (TS_TRUE == pstPara->bThreadStart) continue;
            else
                break;
        } else {
            for (i = 0; i < s32ChnTotal; i++) {
                consume_type = stJpegEncodeCtxArr[i].stream_thread_para.consume_type;
                if (FD_ISSET(VencFd[i], &read_fds)) {
                    /*******************************************************
                     step 2.1 : query how many packs in one-frame stream.
                    *******************************************************/
                    memset(&stStream, 0, sizeof(stStream));

                    s32Ret = TS_MPI_VENC_QueryStatus(i, &stStat);
                    if (TS_SUCCESS != s32Ret) {
                        JPEG_PRT("TS_MPI_VENC_QueryStatus chn[%d] failed with %#x!\n", i, s32Ret);
                        break;
                    }

                    /*******************************************************
                    step 2.2 :suggest to check both u32CurPacks and u32LeftStreamFrames at the same time,for example:
                     if(0 == stStat.u32CurPacks || 0 == stStat.u32LeftStreamFrames)
                     {
                        JPEG_PRT("NOTE: Current  frame is NULL!\n");
                        continue;
                     }
                    *******************************************************/

                    if (0 == stStat.u32CurPacks) {
                        JPEG_PRT("NOTE: Current  frame is NULL!\n");
                        continue;
                    }
                    /*******************************************************
                     step 2.3 : malloc corresponding number of pack nodes.
                    *******************************************************/
                    stStream.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (NULL == stStream.pstPack) {
                        JPEG_PRT("malloc stream pack failed!\n");
                        break;
                    }

                    /*******************************************************
                     step 2.4 : call mpi to get one-frame stream
                    *******************************************************/
                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32Ret = TS_MPI_VENC_GetStream(i, &stStream, TS_TRUE);
                    if (TS_SUCCESS != s32Ret) {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        JPEG_PRT("TS_MPI_VENC_GetStream failed with %#x!\n", s32Ret);
                        break;
                    }

                    /*******************************************************
                     step 2.5 : save frame to file
                    *******************************************************/
                    if (PT_JPEG == enPayLoadType[i]) {
                        if (consume_type == JPEG_FILE || consume_type == JPEG_BOTH) {
                            // snprintf(aszFileName[i], 128, "%s/jpeg_image_chn%d_%d%s", g_jenc_save_path, i, u32PictureCnt[i],szFilePostfix);

                            snprintf(aszFileName[i], 256, "%s", &s_pImageFileName[i][0]);

                            pFile[i] = fopen(aszFileName[i], "wb");
                            if (!pFile[i]) {
                                JPEG_PRT("open file err!\n");
                                return NULL;
                            }
                        }
                    }
                    if (consume_type == JPEG_FILE || consume_type == JPEG_BOTH) {
                        s32Ret = JPEG_SaveStream(pFile[i], &stStream);

                        if (TS_SUCCESS != s32Ret) {
                            free(stStream.pstPack);
                            stStream.pstPack = NULL;
                            JPEG_PRT("save stream failed!\n");
                            break;
                        }
                    }

                    if (consume_type == JPEG_COPY || consume_type == JPEG_BOTH) {
                        s32Ret = JPEG_CopyStream(s_pImageOutBuffer[i], (int *)s_pImageOutBufferSize[i], &stStream);
                    }
                    sem_post(&pstPara->stream_sem);
                    /*******************************************************
                     step 2.6 : release stream
                     *******************************************************/
                    s32Ret = TS_MPI_VENC_ReleaseStream(i, &stStream);
                    if (TS_SUCCESS != s32Ret) {
                        JPEG_PRT("TS_MPI_VENC_ReleaseStream failed!\n");
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        break;
                    }

                    /*******************************************************
                     step 2.7 : free pack nodes
                    *******************************************************/
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    u32PictureCnt[i]++;
                    if (PT_JPEG == enPayLoadType[i]) {
                        if (consume_type == JPEG_FILE || consume_type == JPEG_BOTH) { fclose(pFile[i]); }
                    }
                }
            }
        }
    }

    /*******************************************************
    * step 3 : close save-file
    *******************************************************/
    for (i = 0; i < s32ChnTotal; i++) {
        if (PT_JPEG != enPayLoadType[i]) {
            if (consume_type == JPEG_FILE || consume_type == JPEG_BOTH) { fclose(pFile[i]); }
        }
    }

    return NULL;
}

/******************************************************************************
* funciton : start get jpeg stream process thread
******************************************************************************/
int creat_jpeg_get_stream_thread(JPEG_ENCODE_CONTEXT_S *pstJpegEncodeCtxArr) {

    CHECK_NULL_PTR(pstJpegEncodeCtxArr);

    return pthread_create(&pstJpegEncodeCtxArr->stream_pid, 0, get_jpeg_stream_proc,
                          (void *)&pstJpegEncodeCtxArr->stream_thread_para);
}

/******************************************************************************
* funciton : stop get jpeg stream process.
******************************************************************************/

int destory_jpeg_get_stream_thread(JPEG_ENCODE_CONTEXT_S *pstJpegEncodeCtxArr) {
    CHECK_NULL_PTR(pstJpegEncodeCtxArr);
    if (TS_TRUE == pstJpegEncodeCtxArr->stream_thread_para.bThreadStart) {
        pstJpegEncodeCtxArr->stream_thread_para.bThreadStart = TS_FALSE;
        pthread_join(pstJpegEncodeCtxArr->stream_pid, 0);
    }

    return TS_SUCCESS;
}

int destory_jpeg_send_frame_thread(JPEG_ENCODE_CONTEXT_S *pstJpegEncodeCtxArr) {
    CHECK_NULL_PTR(pstJpegEncodeCtxArr);
    if (TS_TRUE == pstJpegEncodeCtxArr->frame_thread_para.is_frame_enable_run) {
        pstJpegEncodeCtxArr->frame_thread_para.is_frame_enable_run = TS_FALSE;
        pthread_join(pstJpegEncodeCtxArr->frame_pid, 0);
    }

    return TS_SUCCESS;
}

/******************************************************************************
* funciton : Stop jpeg ( stream mode -- , MJPEG )
******************************************************************************/
int jpeg_stop(VENC_CHN VencChn) {
    TS_S32 s32Ret;
    /******************************************
     step 1:  Stop Recv Pictures
    ******************************************/
    s32Ret = TS_MPI_VENC_StopRecvFrame(VencChn);
    if (TS_SUCCESS != s32Ret) {
        JPEG_PRT("TS_MPI_VENC_StopRecvPic vechn[%d] failed with %#x!\n", VencChn, s32Ret);
        return TS_FAILURE;
    } /******************************************
     step 2:  Distroy jpeg Channel
    ******************************************/
    s32Ret = TS_MPI_VENC_DestroyChn(VencChn);
    if (TS_SUCCESS != s32Ret) {
        JPEG_PRT("TS_MPI_VENC_DestroyChn vechn[%d] failed with %#x!\n", VencChn, s32Ret);
        return TS_FAILURE;
    }
    return TS_SUCCESS;
}

void set_jpeg_save_path(unsigned char *path) {
    FILE *fp = fopen((char *)path, "r");
    if (NULL == path || NULL == fp) goto exit;

    strncpy((char *)g_jenc_save_path, (char *)path, 100);
    fclose(fp);
    return;
exit:
    strncpy((char *)g_jenc_save_path, "./", 5);
    return;
}

unsigned char *get_jpeg_save_path(unsigned char *path) { return g_jenc_save_path; }

namespace codec {

static std::mutex jpeg_mutex;
static bool channel_is_enable[JPEG_MAX_CHN_NUM] = {true, true, true, true, true, true, true, true};

TsingJpegEncode::TsingJpegEncode() { init_para_.jpeg_chn = -1; }

TsingJpegEncode::~TsingJpegEncode() {
    if (init_para_.jpeg_chn != -1) {
        std::lock_guard<std::mutex> lock(jpeg_mutex);
        jpeg_encode_deinit(init_para_.jpeg_chn);
        channel_is_enable[init_para_.jpeg_chn] = true;
    }
}

bool TsingJpegEncode::init_codecer(const uint32_t width, const uint32_t height) {
    init_para_.jpeg_chn = -1;

    {
        std::lock_guard<std::mutex> lock(jpeg_mutex);
        for (int i = 0; i < JENC_MAX_NUM; i++) {
            if (channel_is_enable[i] == true) {
                init_para_.jpeg_chn = i;
                channel_is_enable[i] = false;
                break;
            }
        }
    }

    if (init_para_.jpeg_chn == -1) { return false; }

    init_para_.image_width = width;
    init_para_.image_height = height;
    init_para_.consume_type = JPEG_COPY;
    jpeg_encode_init(&init_para_);

    return true;
}

uint32_t TsingJpegEncode::codec_image(const std::shared_ptr<AVFrame> &frame, std::vector<uint8_t> &vec_jpeg_data) {
    JPEG_COMPRESS_S jpeg_data;
    jpeg_data.jpeg_chn = init_para_.jpeg_chn;
    jpeg_data.frame_in = frame.get();
    jpeg_data.out_image_buffer = vec_jpeg_data.data();
    jpeg_data.out_image_size = frame->width * frame->height;
    jpeg_encode_convert(&jpeg_data);

    return jpeg_data.out_image_size;
}

bool TsingJpegEncode::save_image(const std::shared_ptr<AVFrame> &frame, const std::string &path) {
    auto vec_jpeg_data = std::vector<uint8_t>(frame->width * frame->height);
    JPEG_COMPRESS_S jpeg_data;
    jpeg_data.jpeg_chn = init_para_.jpeg_chn;
    jpeg_data.frame_in = frame.get();
    jpeg_data.out_image_buffer = vec_jpeg_data.data();
    jpeg_data.out_image_size = frame->width * frame->height;
    jpeg_encode_convert(&jpeg_data);

    FILE *fp = fopen(path.c_str(), "wb");
    if (fp == nullptr) { return false; }
    fwrite(vec_jpeg_data.data(), 1, jpeg_data.out_image_size, fp);
    fclose(fp);

    return true;
}

}// namespace codec