/*
 * Copyright (C) Beijing Tsing Micro Co.,Ltd. All rights reserved.
 * Description:
 * Author: Tsing Micro solution-app group
 * Create: 2023/04/09
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>

#include "libavformat/avformat.h"

// #include "ts_common.h"
// #include "ts_buffer.h"
// #include "ts_comm_sys.h"
// #include "ts_comm_vb.h"
// #include "ts_comm_vdec.h"
// #include "ts_defines.h"
// #include "mpi_sys.h"
#include "mpi_vb.h"
// #include "mpi_vdec.h"
#include "ts_math.h"
// #include "semaphore.h"
// #include "ts_type.h"
#include "mpi_vgs.h"

#include "tscv.hpp"
#include "tscv_operator.h"

TS_FLOAT TSCV_GetBitSizeByFmt(PIXEL_FORMAT_E pixel_fmt)
{
	TS_FLOAT fVal = 0;
	switch (pixel_fmt)
	{
	case PIXEL_FORMAT_ARGB_8888:
	case PIXEL_FORMAT_ABGR_8888:
	case PIXEL_FORMAT_RGBA_8888:
	case PIXEL_FORMAT_BGRA_8888:
		fVal = 4.0;
		break;
	case PIXEL_FORMAT_YVU_SEMIPLANAR_420:
	case PIXEL_FORMAT_YUV_SEMIPLANAR_420:
	case PIXEL_FORMAT_NV_12:
	case PIXEL_FORMAT_NV_21:
		fVal = 1.5;
		break;
	default:
		break;
	}
	return fVal;
}

void TSCV_CreateVb(TS_S32 pa_w, TS_S32 pa_h, PIXEL_FORMAT_E fmt, TS_U64 *u64PhyAddr, TS_U64 *u64VirAddr)
{
    VB_POOL_CONFIG_S stVbPoolCfg;
    uint32_t vbPool = 0;
    TS_S32 u32HeadStride = ALIGN_UP(pa_w, DEFAULT_ALIGN);
    TS_S32 nlinesize = pa_w*pa_h;
    TS_S32 nsize = nlinesize*TSCV_GetBitSizeByFmt(fmt);
    stVbPoolCfg.u64BlkSize = nsize;
    stVbPoolCfg.u32BlkCnt = 1;
    vbPool = TS_MPI_VB_CreatePool(&stVbPoolCfg);
    VB_BLK VbHandle = TS_MPI_VB_GetBlock(vbPool, nsize, TS_NULL);
    if (VB_INVALID_HANDLE == VbHandle)
    {
        printf("TS_MPI_VB_GetBlock failed!,size %d\n", nsize);
        return;
    }
    *u64PhyAddr = TS_MPI_VB_Handle2PhysAddr(VbHandle);
    TS_S32 ret = TS_MPI_VB_MmapPool(vbPool);
    void *viraddr = NULL;
    TS_MPI_VB_GetBlockVirAddr(vbPool, *u64PhyAddr, &viraddr);
    *u64VirAddr = (TS_U64)viraddr;
}

void TSCV_ReleaseVb(TS_U64 phy_addr)
{
    VB_BLK blk_id =  TS_MPI_VB_PhysAddr2Handle(phy_addr);
    VB_POOL pool_id = TS_MPI_VB_Handle2PoolId(blk_id);
    TS_S32 ret = TS_MPI_VB_MunmapPool(pool_id);
    ret = TS_MPI_VB_DestroyPool(pool_id);
}


static long crop_get_system_time_us()
{
	struct timeval tv1;
	gettimeofday(&tv1, NULL);
	return (long)(tv1.tv_sec*1000000) + (tv1.tv_usec);
}


void* get_vpss_handle(int src_w, int src_h, int dst_w, int dst_h)
{
		void* vpssHandle = NULL;
    cv::Size srcVpssSize(src_w, src_h);
		cv::Size dstVpssSize(dst_w, dst_h);
		tscv::tsImgType srcType = tscv::TS_IMGTYPE_YUV420SP_NV12, dstType = tscv::TS_IMGTYPE_YUV420SP_NV12;

 		vpssHandle = tscv::createVpss(srcVpssSize, dstVpssSize, srcType, dstType);

    return vpssHandle;

}


int tscv_vgs_crop_nv12(AVFrame *frame, TSCV_CROP_S crop_attr) 
{

	VIDEO_FRAME_INFO_S *video_frame_crop = (VIDEO_FRAME_INFO_S *)frame->buf[0]->data;
	TS_S32 src_w = crop_attr.src_w;
	TS_S32 src_h = crop_attr.src_h;
	TS_S32 dst_w = crop_attr.dst_w;
	TS_S32 dst_h = crop_attr.dst_h;
	cv::Size srcSize(src_w, src_h * 3 / 2);
	cv::Size dstSize(dst_w, dst_h * 3 / 2);
	cv::Mat yuvIn(srcSize, CV_8UC1);
	cv::Mat yuvOut(dstSize, CV_8UC1);
	cv::Rect rect(crop_attr.crop_rect.x, crop_attr.crop_rect.y, crop_attr.crop_rect.w, crop_attr.crop_rect.h);
    long t0,t1 = 0;
		
	yuvIn.mpi_phyaddr = video_frame_crop->stVFrame.u64PhyAddr[0];
	yuvIn.mpi_viraddr = video_frame_crop->stVFrame.u64VirAddr[0];

    yuvOut.mpi_phyaddr = crop_attr.dst_phy_addr;
    yuvOut.mpi_viraddr = crop_attr.dst_vir_addr;
	yuvIn.data = (uchar *)yuvIn.mpi_viraddr;
	yuvOut.data = (uchar *)yuvOut.mpi_viraddr;

    t0 = crop_get_system_time_us();
    tscv::cropVgs(yuvIn, yuvOut, rect, 0, 0, tscv::TS_IMGTYPE_YUV420SP_NV12);
    t1 = crop_get_system_time_us();
	printf("cropVgs time:[%ld]us\n", t1 - t0);

    return 0;

}


int tscv_vpss_crop_nv12(AVFrame *frame, void* vpss_handle,  TSCV_CROP_S crop_attr) 
{

	VIDEO_FRAME_INFO_S *video_frame_crop = (VIDEO_FRAME_INFO_S *)frame->buf[0]->data;
	TS_S32 src_w = crop_attr.src_w;
	TS_S32 src_h = crop_attr.src_h;
	TS_S32 dst_w = crop_attr.dst_w;
	TS_S32 dst_h = crop_attr.dst_h;
	cv::Size srcSize(src_w, src_h * 3 / 2);
	cv::Size dstSize(dst_w, dst_h * 3 / 2);
	cv::Mat yuvIn(srcSize, CV_8UC1);
	cv::Mat yuvOut(dstSize, CV_8UC1);
	cv::Rect rect(crop_attr.crop_rect.x, crop_attr.crop_rect.y, crop_attr.crop_rect.w, crop_attr.crop_rect.h);
    long t0,t1 = 0;
		
	yuvIn.mpi_phyaddr = video_frame_crop->stVFrame.u64PhyAddr[0];
	yuvIn.mpi_viraddr = video_frame_crop->stVFrame.u64VirAddr[0];

    yuvOut.mpi_phyaddr = crop_attr.dst_phy_addr;
    yuvOut.mpi_viraddr = crop_attr.dst_vir_addr;
	yuvIn.data = (uchar *)yuvIn.mpi_viraddr;
	yuvOut.data = (uchar *)yuvOut.mpi_viraddr;

    t0 = crop_get_system_time_us();
    tscv::cropVpss(vpss_handle, yuvIn, yuvOut, rect, 0, 0, tscv::TS_IMGTYPE_YUV420SP_NV12);
    t1 = crop_get_system_time_us();
	printf("cropVgs time:[%ld]us\n", t1 - t0);

    return 0;

}




