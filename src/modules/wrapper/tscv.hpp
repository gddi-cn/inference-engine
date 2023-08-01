#ifndef _TS_OPENCV
#define _TS_OPENCV

#ifdef __cplusplus
#endif /* __cplusplus */

#include "opencv2/core.hpp"
#include "opencv2/imgproc/imgproc_c.h"
#include "opencv2/imgproc/imgproc.hpp"
//#include "mat.hpp"
#include "ts_type.h"

using namespace std;
using namespace cv;

namespace tscv {

	typedef enum{
		TS_IMGTYPE_ARGB8888,
		TS_IMGTYPE_ABGR8888,
		TS_IMGTYPE_BGRA8888,
		TS_IMGTYPE_RGBA8888,
		TS_IMGTYPE_YUV422_YUYV,
		TS_IMGTYPE_YUV422_YVYU,
		TS_IMGTYPE_YUV422_UYVY,
		TS_IMGTYPE_YUV422_VYUY,
		TS_IMGTYPE_YUV400,
		TS_IMGTYPE_YUV422SP_NV12,
		TS_IMGTYPE_YUV420SP_NV12,
		TS_IMGTYPE_YUV422SP_NV21,
		TS_IMGTYPE_YUV420SP_NV21,
		TS_IMGTYPE_YUV422_3P,
		TS_IMGTYPE_YUV420_3P,
		TS_IMGTYPE_MAX_NUM
	}tsImgType;

	typedef enum{
		TS_ARGB2YUV420SP,
		TS_YUV420SP2ARGB,
		TS_YUV420SP2RGBA,
		TS_YUV420SP2BGRA
	}tsColorTrans;

	typedef enum{
		TSCV_LBP_CMP_MODE_NORMAL,
		TSCV_LBP_CMP_MODE_ABS,
	}tscvLbpCmpMode;

	typedef enum{
		TSCV_ORD_MODE_MED,
		TSCV_ORD_MODE_MIN,
		TSCV_ORD_MODE_MAX,
	}tscvOrdMode;

	typedef enum {
		NO_BCR_SIMILAR_CONVERT,
		SIMILAR_CONVERT,
		AFFINE_CONVERT,
	} warpAffineModeE;

	/**
	 * @brief 直方图统计运算
	 * @param images 输入图像
	 * @param hist 输出直方图统计结果256*16bit
	 * @param 其他参数暂未使用
	 * @attention 输入输出图像最大分辨率2688*1944;只支持灰度图运算
	*/
	CV_EXPORTS void calcHist( const cv::Mat* images, int nimages,
							  const int* channels, InputArray mask,
							  OutputArray hist, int dims, const int* histSize,
							  const float** ranges, bool uniform = true, bool accumulate = false );

	/**
	 * @brief 两图片/矩阵加运算
	 * @param src1 输入Mat1
	 * @param src2 输入Mat2
	 * @param dst 目标Mat
	 * @param mask 对齐opencv api,暂未使用
	 * @attention 输入输出图像最大分辨率2688*1944;只支持灰度图运算
	*/
	CV_EXPORTS void add(InputArray src1, InputArray src2, OutputArray dst,
                      InputArray mask = noArray(), int dtype = -1);

	/**
	 * @brief 两图片/矩阵加运算
	 * @param src1 输入Mat1
	 * @param src2 输入Mat2
	 * @param dst 目标Mat
	 * @param mask 对齐opencv api,暂未使用
	 * @attention 输入输出图像最大分辨率2688*1944;只支持灰度图运算
	*/
	CV_EXPORTS void subtract(InputArray src1, InputArray src2, OutputArray dst,
                           InputArray mask = noArray(), int dtype = -1);


	/**
	 * @brief 两图片/矩阵与运算
	 * @param src1 输入Mat1
	 * @param src2 输入Mat2
	 * @param dst 目标Mat
	 * @param mask 对齐opencv api,暂未使用
	 * @attention 输入输出图像最大分辨率2688*1944;只支持灰度图运算
	*/
	CV_EXPORTS_W void bitwise_and(InputArray src1, InputArray src2,
                              OutputArray dst, InputArray mask = noArray());

	/**
	 * @brief 两图片/矩阵或运算
	 * @param src1 输入Mat1
	 * @param src2 输入Mat2
	 * @param dst 目标Mat
	 * @param mask 对齐opencv api,暂未使用
	 * @attention 输入输出图像最大分辨率2688*1944;只支持灰度图运算
	*/
	CV_EXPORTS_W void bitwise_or(InputArray src1, InputArray src2,
                             OutputArray dst, InputArray mask = noArray());

	/**
	 * @brief 两图片/矩阵异或运算
	 * @param src1 输入Mat1
	 * @param src2 输入Mat2
	 * @param dst 目标Mat
	 * @param mask 对齐opencv api,暂未使用
	 * @attention 输入输出图像最大分辨率2688*1944;只支持灰度图运算
	*/
	CV_EXPORTS_W void bitwise_xor(InputArray src1, InputArray src2,
                              OutputArray dst, InputArray mask = noArray());

	/**
	 * @brief 图片/矩阵阈值化运算
	 * @param src 输入Mat
	 * @param dst 目标Mat
	 * @param thresh 阈值参数
	 * @maxval 阈值最大值
	 * @type 阈值化类型 (参考 #ThresholdTypes),只支持THRESH_BINARY和THRESH_TRUNC类型
	 * @attention 输入输出图像最大分辨率2688*1944;只支持灰度图运算;只支持U8C1->U8C1的转换
	*/
	CV_EXPORTS_W void threshold(InputArray src, OutputArray dst, double thresh, double maxval, int type);

	/**
	 * @brief 图片/矩阵Map（映射赋值）运算，查找 Map 查找表中的值，赋予目标图像相应像素查找表中的值，
			支持U8C1->U8C1、U8C1->U16C1、U8C1->S16C1 3种模式的映射
	 * @param src 输入Mat
	 * @param dst 目标Mat
	 * @param map map表
	 * @param mapSize map表大小
	 * @attention 输入输出图像最大分辨率2688*1944;只支持灰度图运算;mapSize最大为256
	*/
	CV_EXPORTS_W void map(InputArray src, OutputArray dst, unsigned short *map, unsigned short mapSize);

	/**
	 * @brief vpss初始化函数，为避免重复初始化，提升算法效率，进程开始时进行vpss初始化工作
	 * @param srcSize 源分辨率，受逻辑影响，初始化时需要指定源分辨率，后续优化
	 * @param dstSize 目的分辨率，受逻辑影响，初始化时需要指定目的分辨率，后续优化
	 * @param srcType 输入的图像格式 默认格式为nv12，可选项nv12、argb、abgr、bgra、rgba，受逻辑影响，初始化时需要指定源type，后续优化
	 * @param dstType 输出的图像格式 默认格式为nv12，可选项nv12、argb、abgr、bgra、rgba，受逻辑影响，初始化时需要指定目的type，后续优化
	 * @return  createVpss生成的句柄,用以获取不同句柄实现接口并行执行
	*/
	CV_EXPORTS void* createVpss(cv::Size srcSize, cv::Size dstSize, tsImgType srcType = TS_IMGTYPE_YUV420SP_NV12, tsImgType dstType = TS_IMGTYPE_YUV420SP_NV12 );

	/**
	 * @brief vpss反初始化函数，为避免重复初始化，提升算法效率，进程开始时进行一次vpss初始化工作，进程结束时进行一次vpss反初始化操作
	* @param vpssHandle createVpss生成的句柄,用以获取不同句柄实现接口并行执行
	*/
	CV_EXPORTS void exitVpss(void* vpssHandle);

	/**
	 * @brief 缩放函数
	 * @param vpssHandle createVpss生成的句柄,用以获取不同句柄实现接口并行执行
	 * @param src 源Mat
	 * @param dst 目标Mat
	 * @param dsize 要缩放到的大小,w/h限制[0-8192]
	 * @param type 输入的图像格式 默认格式为nv12，可选项nv12、argb、abgr、bgra、rgba
	 * @param interpolation 停用字段，暂不支持
	*/
	CV_EXPORTS void resizeVpss(void* vpssHandle, cv::Mat &src, cv::Mat &dst, cv::Size dsize, tsImgType type = TS_IMGTYPE_YUV420SP_NV12,
							int interpolation = CV_INTER_LINEAR );
	/**
	 * @brief 图像格式转换 目前支持nv12与argb、nv12与abgr、nv12与bgra、nv12与rgba互转
	 * @param vpssHandle createVpss生成的句柄,用以获取不同句柄实现接口并行执行
	 * @param src 源Mat
	 * @param dst 目标Mat
	 * @param code 转换参数
	*/
	CV_EXPORTS void cvtColorVpss(void* vpssHandle, cv::Mat &src, cv::Mat &dst, tsColorTrans code);

	/**
	 * @brief 图像缩放格式转换 目前支持nv12与argb、nv12与abgr、nv12与bgra、nv12与rgba互转
	 * @param vpssHandle createVpss生成的句柄,用以获取不同句柄实现接口并行执行
	 * @param src 源Mat
	 * @param dst 目标Mat
	 * @param dsize 要缩放到的大小,w/h限制[0-8192]
	 * @param code 转换参数
	 * @param interpolation 停用字段，暂不支持
	*/
	CV_EXPORTS void resizeCvtColorVpss(void* vpssHandle, cv::Mat &src, cv::Mat &dst, cv::Size dsize, tsColorTrans code,
							int interpolation = CV_INTER_LINEAR);

	/**
	 * @brief 裁剪函数
	 * @param vpssHandle createVpss生成的句柄,用以获取不同句柄实现接口并行执行
	 * @param src 源Mat
	 * @param dst 目标Mat
	 * @param srcRect 要裁剪的矩形
	 * @param dst_x 将裁剪后的矩形放入dst的左上角坐标x
	 * @param dst_y 将裁剪后的矩形放入dst的左上角坐标y
	 * @param type 输入的图像格式 默认格式为nv12，可选项nv12、argb、abgr、bgra、rgba
	*/
	CV_EXPORTS void cropVpss(void* vpssHandle, cv::Mat &src, cv::Mat &dst, cv::Rect srcRect, int dst_x= 0, int dst_y = 0, tsImgType type = TS_IMGTYPE_YUV420SP_NV12);

	/**
	 * @brief 裁剪缩放格式转换函数
	 * @param vpssHandle createVpss生成的句柄,用以获取不同句柄实现接口并行执行
	 * @param src 源Mat
	 * @param dst 目标Mat
	 * @param srcRect 要裁剪的矩形
	 * @param dst_x 将裁剪后的矩形放入dst的左上角坐标x
	 * @param dst_y 将裁剪后的矩形放入dst的左上角坐标y
	 * @param src_type 输入的图像格式 可选项nv12、argb、abgr、bgra、rgba
	 * @param dst_type 输出的图像格式 可选项nv12、argb、abgr、bgra、rgba
	*/
	CV_EXPORTS void cropResizeVpss(void* vpssHandle, cv::Mat &src, cv::Mat &dst, cv::Rect srcRect, cv::Size dsize, tsImgType srcType, tsImgType dstType);

	/**
	 * @brief 镜像翻转
	 * @param vpssHandle createVpss生成的句柄,用以获取不同句柄实现接口并行执行
	 * @param src 源Mat
	 * @param dst 目标Mat
	 * @param direction 方向,小于0(例如-1)代表左右上下颠倒，0代表上下颠倒，大于0(例如1)代表左右颠倒
	 * @param type 输入的图像格式 默认格式为nv12，可选项nv12、argb、abgr、bgra、rgba
	*/
	CV_EXPORTS void flipVpss(void* vpssHandle, cv::Mat &src, cv::Mat &dst,  int direction = 0, tsImgType type = TS_IMGTYPE_YUV420SP_NV12);

	/**
	 * @brief 缩放函数，目前nv12需要输入输出图像宽需要64字节对齐
	 * @param src 源Mat
	 * @param dst 目标Mat
	 * @param dsize 要缩放到的大小
	 * @param type 输入的图像格式 默认格式为nv12，可选项nv12、argb，后续支持待测
	 * @param interpolation 停用字段，暂不支持
	*/
	CV_EXPORTS void resizeVgs(cv::Mat &src, cv::Mat &dst, cv::Size dsize, tsImgType type = TS_IMGTYPE_YUV420SP_NV12,
							int interpolation = CV_INTER_LINEAR );
	/**
	 * @brief 图像格式转换 目前仅支持nv12与argb互转
	 * @param src 源Mat
	 * @param dst 目标Mat
	 * @param code 转换参数
	*/
	CV_EXPORTS void cvtColorVgs(cv::Mat &src, cv::Mat &dst, tsColorTrans code);

	/**
	 * @brief 图像缩放格式转换 目前仅支持nv12与argb互转
	 * @param src 源Mat
	 * @param dst 目标Mat
	 * @param dsize 要缩放到的大小
	 * @param code 转换参数
	 * @param interpolation 停用字段，暂不支持
	*/
	CV_EXPORTS void resizeCvtColorVgs(cv::Mat &src, cv::Mat &dst, cv::Size dsize, tsColorTrans code,
							int interpolation = CV_INTER_LINEAR);

	/**
	 * @brief 图像padding 目前仅支持nv12与argb互转
	 * @param src 源Mat
	 * @param dst 目标Mat _dst.w=_src.w+left+right  _dst_h=_src_h+top+bottom
	 * @param top padding_top的大小
	 * @param bottom padding_bottom的大小
	 * @param left padding_left的大小
	 * @param right padding_right的大小
	 * @param bg_color padding填充颜色
	 * @param type 图像格式类型
	*/
	CV_EXPORTS void copyMakeBorderVgs(cv::Mat &src, cv::Mat &dst, int top, int bottom, int left, int right,
							int bgColor, tsImgType type = TS_IMGTYPE_YUV420SP_NV12);

	/**
	 * @brief 裁剪函数，目前nv12需要输入输出图像宽需要64字节对齐, x,y需要是4的倍数
	 * @param src 源Mat
	 * @param dst 目标Mat
	 * @param srcRect 要裁剪的矩形
	 * @param dst_x 将裁剪后的矩形放入dst的左上角坐标x
	 * @param dst_y 将裁剪后的矩形放入dst的左上角坐标y
	 * @param type 输入的图像格式 默认格式为nv12，可选项nv12、argb，后续支持待测
	*/
	CV_EXPORTS void cropVgs(cv::Mat &src, cv::Mat &dst, cv::Rect srcRect, int dst_x= 0, int dst_y = 0, tsImgType type = TS_IMGTYPE_YUV420SP_NV12);

	/**
	 * @brief 裁剪函数，目前nv12需要输入输出图像宽需要64字节对齐, x,y需要是4的倍数
	 * @param src 源Mat
	 * @param dst 目标Mat
	 * @param direction 方向
	 * @param type 输入的图像格式 默认格式为nv12，可选项nv12、argb，后续支持待测
	*/
	CV_EXPORTS void flipVgs(cv::Mat &src, cv::Mat &dst,  int direction = 0, tsImgType type = TS_IMGTYPE_YUV420SP_NV12);

	/**
	 * @brief 裁剪缩放格式转换函数，目前nv12需要输入输出图像宽需要64字节对齐, x,y需要是4的倍数
	 * @param src 源Mat
	 * @param dst 目标Mat
	 * @param srcRect 要裁剪的矩形
	 * @param dst_x 将裁剪后的矩形放入dst的左上角坐标x
	 * @param dst_y 将裁剪后的矩形放入dst的左上角坐标y
	 * @param src_type 输入的图像格式 可选项nv12、argb，后续支持待测
	 * @param dst_type 输出的图像格式 可选项nv12、argb，后续支持待测
	*/
	CV_EXPORTS void cropResizeVgs(cv::Mat &src, cv::Mat &dst, cv::Rect srcRect, cv::Size dsize, tsImgType srcType, tsImgType dstType);

	/** @brief Converts an array from s16 to u8 data type with optional scaling.

	The method converts source pixel values to the u8.

	\f[dst(x,y) = saturate \_ cast<rType>( (\alpha (src)(x,y)) +  \beta )\f]
	@param src input matrix; support CV_16S, resolution should > 64*64
	@param dst output matrix; support CV_8U
	@param rtype desired output matrix type, support CV_8U
	@param alpha optional scale factor, alpha >= 0.0, alpha_used = fixed(alpha)
	@param beta optional delta added to the scaled values, beta_used = fixed(beta)
	*/
	void convertTo(InputArray src, OutputArray dst, int rtype, double alpha=1.0, double beta=0.0);

	/**
	 * @brief SAD匹配运算
	 * @param src1 输入image1
	 * @param src2 输入image2
	 * @param resultSad SAD结果输出
	 * @param resultThre 匹配结果输出
	 * @attention 输入输出图像最大分辨率2688*1944;只支持灰度图运算
	*/
	CV_EXPORTS_W void sadMatchRce(InputArray src1, InputArray src2, OutputArray resultSad, OutputArray resultThre);

	/**
	 * @brief 计算两图归一化系数
	 * @param src1 输入src1
	 * @param src2 输入src2
	 * @param result 计算中间结果
	 * @param out 归一化计算结果
	 * @attention 输入输出图像最大分辨率2688*1944;只支持灰度图运算
	 */
	CV_EXPORTS_W void calcNcc(InputArray src1, InputArray src2, OutputArray result, double *out);

	/**
	 * @brief 计算两图视差图
	 * @param src1 输入src1
	 * @param src2 输入src2
	 * @param result 计算结果视差图
	 * @attention 输入输出图像最大分辨率2688*1944;只支持灰度图运算
	 */
	CV_EXPORTS_W void calcNccMatch(InputArray src1, InputArray src2, OutputArray result);

	/**
	 * @brief 两图匹配运算
	 * @param src1 输入src1
	 * @param src2 输入src2
	 * @param result 匹配结果
	 * @param thresholdMatch 阈值
	 * @attention 输入输出图像最大分辨率2688*1944;只支持灰度图运算
	 */
	CV_EXPORTS_W void featMatch(InputArray src1, InputArray src2, OutputArray result, TS_U16 thresholdMatch);

	/** @brief Calculates the first image derivatives using an extended Sobel operator.

	for the x-derivative, or transposed for the y-derivative.

	The function calculates an image derivative by convolving the image with the appropriate kernel:

	\f[\texttt{dst} =  \frac{\partial^{xorder+yorder} \texttt{src}}{\partial x^{xorder} \partial y^{yorder}}\f]

	The Sobel operators combine Gaussian smoothing and differentiation, so the result is more or less
	resistant to the noise. Most often, the function is called with ( xorder = 1, yorder = 0, ksize = 3)
	or ( xorder = 0, yorder = 1, ksize = 3) to calculate the first x- or y- image derivative. The first
	case corresponds to a kernel of:

	\f[\vecthreethree{-1}{0}{1}{-2}{0}{2}{-1}{0}{1}\f]

	The second case corresponds to a kernel of:

	\f[\vecthreethree{-1}{-2}{-1}{0}{0}{0}{1}{2}{1}\f]

	@param src input image, rows:[64, 2048]; cols:[64, 2048], only CV_8UC1
	@param dst output image of the same size and the same number of channels as src.
	@param ddepth output image depth, only CV_16SC1
	@param dx order of the derivative x, only support 0, 1.
	@param dy order of the derivative y, only support 0, 1.
	@param ksize size of the extended Sobel kernel; it must be 3, 5.
	@param scale optional scale factor for the computed derivative values; by default, no scaling is
	applied
			0.75         0.375         0.1875   0.09375       0.046875
	scale	-------+-----+------+------+--------+----+---+----+------+-----+-----+
		1           0.5            0.25    0.125       0.0625       0.03125
	proc	-----1.0-----+----0.5------+----0.25-----+--0.125-+---0.0626---+---0.03125
	@param delta unused.
	@param borderType unused, borderType is BORDER_REFLECT101.
	*/
	void Sobel(InputArray src, OutputArray dst, int ddepth, int dx, int dy, int ksize = 3, double scale = 1, double delta = 0, int borderType = BORDER_DEFAULT);

	/** @brief Finds edges in an image using the Canny algorithm(sobel+threshold).
	 mag = abs(sobel_h) + abs(sobel_v)
	 out = mag >= threshold1 ? mag : 0

	The function finds edges in the input image and marks them in the output map edges using the
	Canny algorithm.
	@param image 8-bit input image, support CV_8UC1, rows:[64, 2048]; cols:[64, 2048].
	@param edges output edge map; single channels 8-bit image, which has the same size as image, support CV_8UC1.
	@param threshold1 threshold_used = fixed(threshold1).
	@param threshold2 unused.
	@param apertureSize aperture size for the Sobel operator, support 3, 5.
	@param L2gradient unused.
	*/
	CV_EXPORTS_W void Canny(InputArray image, OutputArray edges, double threshold1, double threshold2,
                         int apertureSize = 3, bool L2gradient = false);

	/** @brief Dilates an image by using a specific structuring element.

	dst(x,y) =  &(2,-2)[src | kernel]

	@param src input binary image; but the depth should be CV_8UC1, rows:[64, 2048]; cols:[64, 2048].
	@param dst output binary image of the same size and type as src.
	@param kernel structuring element used for dilation, size = 5x5, CV_8UC1
	@param anchor unused, the anchor is at the element center.
	@param iterations unused, iterations number of times dilation is 1.
	@param borderType unused, borderType is BORDER_REFLECT101.
	@param borderValue unused
	*/
	CV_EXPORTS_W void dilate(InputArray src, OutputArray dst, InputArray kernel,
                          Point anchor = Point(-1,-1), int iterations = 1,
                          int borderType = BORDER_CONSTANT,
                          const Scalar& borderValue = morphologyDefaultBorderValue());

	/** @brief Dilates an image by using a specific structuring element.

	dst(x,y) =  I(2,-2)[src & kernel]

	@param src input binary image; but the depth should be CV_8U, rows:[64, 2048]; cols:[64, 2048].
	@param dst output binary image of the same size and type as src.
	@param kernel structuring element used for dilation, size = 5x5, CV_8U
	@param anchor unused, the anchor is at the element center.
	@param iterations unused, iterations number of times dilation is 1.
	@param borderType unused, borderType is BORDER_REFLECT101.
	@param borderValue unused
	*/
	CV_EXPORTS_W void erode(InputArray src, OutputArray dst, InputArray kernel,
                          Point anchor = Point(-1,-1), int iterations = 1,
                          int borderType = BORDER_CONSTANT,
                          const Scalar& borderValue = morphologyDefaultBorderValue());

	/** @brief Convolves an image with the kernel.

	@param src input image, CV_8UC1, rows:[64, 2048]; cols:[64, 2048].
	@param dst output image of the same size and the same number of channels as src, CV_8UC1.
	@param ddepth desired depth of the destination image, only support -1(same depth as the source)
	@param kernel convolution kernel (or rather a correlation kernel), a single-channel floating point
	matrix; each coeff should < 1.0, kernel support 5x5
	@param anchor unused, only support(-1, -1). anchor anchor of the kernel that indicates the relative position of a filtered point within
	the kernel; the anchor should lie within the kernel; default value (-1,-1) means that the anchor
	is at the kernel center.
	@param delta unused, optional value added to the filtered pixels before storing them in dst.
	@param borderType unused, pixel extrapolation method, , support BORDER_REFLECT101.
	*/
	CV_EXPORTS_W void filter2D(InputArray src, OutputArray dst, int ddepth,
				InputArray kernel, Point anchor = Point(-1,-1),
				double delta = 0, int borderType = BORDER_DEFAULT);

	/** @brief Blurs an image using the median filter.

	The function smoothes an image using the median filter with the \f$\texttt{3} \times
	\texttt{3}\f$ aperture.

	@param src, it can only be CV_8UC1, rows:[64, 2048]; cols:[64, 2048].
	@param dst destination array of the same size and type as src.
	@param ksize aperture linear size; only support 3
	*/
	CV_EXPORTS_W void medianBlur(InputArray src, OutputArray dst, int ksize);

	/** @brief LBP(Local Binary Pattern) filter, only support 3*3.

	@param src, it can only be CV_8UC1, rows:[64, 2048]; cols:[64, 2048].
	@param dst destination array of the same size and type as src.
	@param mode TSCV_LBP_CMP_MODE_NORMAL/TSCV_LBP_CMP_MODE_ABS
	@param thresh TSCV_LBP_CMP_MODE_NORMAL:[-128, 127]; TSCV_LBP_CMP_MODE_ABS:[0:255]
	*/
	void LBP(InputArray src, OutputArray dst, tscvLbpCmpMode mode, TS_U8 thresh);

	/** @brief max/min/median value filter, only support 5*5.

	@param src, it can be CV_16UC1, rows:[64, 2048]; cols:[64, 2048].
	@param dst destination array of the same size and type as src.
	@param mode TSCV_ORD_MODE_MED/TSCV_ORD_MODE_MIN/TSCV_ORD_MODE_MAX/TSCV_ORD_MODE_MIN,
	*/
	void ordStatFilter(InputArray src, OutputArray dst, tscvOrdMode mode);

	/**
	 * @brief 仿射变换运算
	 * @param src 源Mat
	 * @param dst 目标Mat
	 * @param pPair 输入点对信息
				点对信息存储内存排列:(unsigned short) p0srcx,p0srcy,p0dstx,p0dsty,p1srcx,p2srcy,p3dstx,p4dsty,......
				点对数量取值范围[3-68]
	 * @param roi 放射变换的roi区域
				roi区域存储内存排列:(unsigned short) roiLeft,roiRight,roiTop,roiBottom,roiStride0,roiStride1,roiDstW,roiDstH,reserve0,reserve1
				roi目前支持一个区域
	 * @param outM 输出的仿射变换矩阵
	 * @attention 输入输出图片宽[64-2160];高[64~3840]
	*/
	CV_EXPORTS_W void warpAffine(InputArray src, OutputArray dst,
										InputArray pPair, InputArray roi, OutputArray outM);

	/**
	 * @brief 透视变换运算
	 * @param src 源Mat
	 * @param dst 目标Mat
	 * @param pPair 输入点对信息
				点对信息存储内存排列:(unsigned short) p0srcx,p0srcy,p0dstx,p0dsty,p1srcx,p2srcy,p3dstx,p4dsty,......
				点对数量取值范围[3-68]
	 * @param roi 透视变换的roi区域
				roi区域存储内存排列:(unsigned short) roiLeft,roiRight,roiTop,roiBottom,roiStride0,roiStride1,roiDstW,roiDstH,reserve0,reserve1
				roi目前支持一个区域
	 * @param outM 输出的透视变换矩阵
	 * @attention 输入输出图片宽[64-2160];高[64~3840]
	*/
	CV_EXPORTS_W void warpPerspective(InputArray src, OutputArray dst,
										InputArray pPair, InputArray roi, OutputArray outM, int mode = AFFINE_CONVERT);
}


#endif /* _TS_OPENCV */
