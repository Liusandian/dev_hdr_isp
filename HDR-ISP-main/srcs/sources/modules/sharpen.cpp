/**
 * @file sharpen.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-27
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 */
// =============================================================================
// 模块: sharpen (锐化 / Unsharp Masking, USM 反锐化掩模)
// -----------------------------------------------------------------------------
// 功能: 增强图像边缘和细节, 使图像看起来更清晰。
//
// 算法原理(USM, Unsharp Masking):
//   1) 对输入图像做高斯低通滤波, 得到模糊图 blur(去除高频细节)
//   2) 反锐化掩模公式: out = (src - w * blur) / (1 - w)
//      其中 w 为锐化强度(0.1~0.9, 越大锐化越强)
//   等价形式: out = src + (w/(1-w)) * (src - blur)
//      即在原图上叠加"原图 - 模糊图"(高频细节)的放大版, 增强边缘对比度。
//
// 高斯核(5x5, 整数近似, 和为 273):
//   {1, 4, 7, 4, 1}
//   {4,16,26,16, 4}
//   {7,26,41,26, 7}
//   {4,16,26,16, 4}
//   {1, 4, 7, 4, 1}
// 数据流向: u8(Y) -> u8(Y), 色彩域保持 YUV(仅锐化亮度, 不影响色度)。
// =============================================================================

/*
USM： (src - w * gauss）/（1 - w）
w: 0.1～0.9, default: 0.6
*/
#include "modules/modules.h"

#define MOD_NAME "sharpen"

const int KernelSum = 273;   // 高斯核元素总和(用于归一化)
// 5x5 高斯低通核(整数近似, 中心对称)
const int kGaussKernel[5][5] = {
    {1, 4, 7, 4, 1},
    {4, 16, 26, 16, 4},
    {7, 26, 41, 26, 7},
    {4, 16, 26, 16, 4},
    {1, 4, 7, 4, 1},
};

// 锐化(USM)主函数
static int Sharpen(Frame *frame, const IspPrms *isp_prm)
{
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }
    int pixel_idx = 0;

    uint8_t *y_i = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_i.y);   // 输入亮度 Y
    uint8_t *y_o = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_o.y);   // 锐化后亮度 Y

    float ratio = isp_prm->sharpen_prms.ratio;   // 锐化强度 w (0.1~0.9)

    FOR_ITER(h, frame->info.height)
    {
        FOR_ITER(w, frame->info.width)
        {
            pixel_idx = h * frame->info.width + w;
            // 边缘 2 像素(5x5 核越界), 直接拷贝不处理
            if ((w < 2) || (h < 2) || (w > (frame->info.width - 3)) || (h > (frame->info.height - 3)))
            {
                y_o[pixel_idx] = y_i[pixel_idx];
                continue;
            }

            int y = 0;

            // 5x5 高斯低通滤波: 加权求和
            for (int kh = h - 2, gauss_idy = 0; kh <= h + 2; ++kh, ++gauss_idy)
            {
                for (int kw = w - 2, gauss_idx = 0; kw <= w + 2; ++kw, ++gauss_idx)
                {
                    y += (y_i[GET_PIXEL_INDEX(kw, kh, frame->info.width)] * kGaussKernel[gauss_idy][gauss_idx]);
                }
            }
            // 归一化得到模糊值 blur
            y = y / KernelSum;

            // USM 公式: out = (src - w * blur) / (1 - w)
            //   等价于 src + (w/(1-w)) * (src - blur), 放大高频细节
            y = static_cast<int>((y_i[pixel_idx] - ratio * y) / (1 - ratio));

            ClipMinMax(y, 255, 0);
            y_o[pixel_idx] = y;
        }
    }

    SwapMem<void>(frame->data.yuv_u8_i.y, frame->data.yuv_u8_o.y);

    return 0;
}

// 模块注册: s32 -> s32, YUV -> YUV
void RegisterSharpenMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_INT32;
    mod.out_type = DataPtrTypes::TYPE_INT32;

    mod.in_domain = ColorDomains::YUV;
    mod.out_domain = ColorDomains::YUV;

    mod.name = MOD_NAME;
    mod.run_function = Sharpen;

    RegisterIspModule(mod);
}