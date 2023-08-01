/*
 * Copyright (C) Beijing Tsing Micro Co.,Ltd. All rights reserved.
 * Description:
 * Author: Tsing Micro solution-app group
 * Create: 2023/04/12
 */
#ifndef __TSCV_OPERATOR_H__
#define __TSCV_OPERATOR_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#include "ts_type.h"


// #ifndef __KERNEL__
// #include <sys/time.h>
// #endif

// #include <opencv2/core/core.hpp>
// #include <opencv2/highgui/highgui.hpp>
// #include <opencv2/imgproc/imgproc.hpp>
// #include <stdint.h>
#include "ts_comm_video.h"
#include "libavformat/avformat.h"

typedef struct _CV_RECT
{
    int x;
    int y;
    int w;
    int h;
} CV_RECT_S;

typedef struct TSCV_CROP
{
    int src_w ;
    int src_h ;
    int dst_w ;
    int dst_h ;  
    CV_RECT_S crop_rect;
    TS_U64 dst_vir_addr;
    TS_U64 dst_phy_addr;    
}TSCV_CROP_S;

void TSCV_ReleaseVb(TS_U64 phy_addr);
void TSCV_CreateVb(TS_S32 pa_w, TS_S32 pa_h, PIXEL_FORMAT_E fmt, TS_U64 *u64PhyAddr, TS_U64 *u64VirAddr);

// // int tscv_vgs_crop (AVFrame *frame); 
int tscv_vgs_crop_nv12(AVFrame *frame, TSCV_CROP_S crop_attr);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif
