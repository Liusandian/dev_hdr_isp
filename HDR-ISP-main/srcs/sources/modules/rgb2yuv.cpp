/**
 * @file bgr2yuv.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-27
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 */
// =============================================================================
// 模块: rgb2yuv (BGR -> YUV 色彩空间转换)
// -----------------------------------------------------------------------------
// 功能: 将 BGR 三通道图像转换为 YUV 色彩空间, 便于后续在 YUV 域处理亮度与色度
//       分离的算法(伽马、对比度、锐化、降噪、饱和度等)。
//
// 背景: YUV 将亮度(Y)与色度(U/V)分离, 人眼对亮度敏感、对色度不敏感, 因此 YUV
//       域处理可单独优化亮度(不影响颜色)或色度(不影响细节), 且便于视频压缩。
//
// 转换公式(BT.601 标准, 8bit):
//   Y  =  0.299*R + 0.587*G + 0.114*B              (亮度, 范围 0~255)
//   U  = -0.147*R - 0.289*G + 0.436*B + 128        (色差 B-Y, 中心 128)
//   V  =  0.615*R - 0.515*G - 0.100*B + 128        (色差 R-Y, 中心 128)
// 数据流向: s32(BGR) -> u8(YUV), 色彩域由 BGR 转为 YUV。
// =============================================================================

#include "modules/modules.h"

#define MOD_NAME "rgb2yuv"

// BGR -> YUV 主函数
static int Rgb2Yuv(Frame *frame, const IspPrms *isp_prm)
{
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }
    int pixel_idx = 0;
    int pwl_idx = 0;

    int32_t *bgr_i = reinterpret_cast<int32_t *>(frame->data.bgr_s32_i);   // RGB 伽马后 BGR 输入
    uint8_t *y_o = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_o.y);    // 亮度 Y 输出
    uint8_t *u_o = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_o.u);    // 色度 U 输出
    uint8_t *v_o = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_o.v);    // 色度 V 输出

    const DePwlPrms *pwl_prm = &(isp_prm->depwl_prm);

    FOR_ITER(ih, frame->info.height)
    {
        FOR_ITER(iw, frame->info.width)
        {
            int pixel_idx = GET_PIXEL_INDEX(iw, ih, frame->info.width);

            // 取出 BGR 分量(存储顺序 B, G, R)
            auto b = bgr_i[3 * pixel_idx + 0];
            auto g = bgr_i[3 * pixel_idx + 1];
            auto r = bgr_i[3 * pixel_idx + 2];

            // BT.601 YUV 转换: Y 亮度, U/V 色差(中心 128)
            auto y = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
            auto u = static_cast<uint8_t>(-0.147 * r - 0.289 * g + 0.436 * b + 128);
            auto v = static_cast<uint8_t>(0.615 * r - 0.515 * g - 0.100 * b + 128);

            y_o[pixel_idx] = y;
            u_o[pixel_idx] = u;
            v_o[pixel_idx] = v;
        }
    }

    // 分别交换 Y/U/V 三平面缓冲
    SwapMem<void>(frame->data.yuv_u8_o.y, frame->data.yuv_u8_i.y);
    SwapMem<void>(frame->data.yuv_u8_o.u, frame->data.yuv_u8_i.u);
    SwapMem<void>(frame->data.yuv_u8_o.v, frame->data.yuv_u8_i.v);

    return 0;
}

// 模块注册: s32 -> s32, BGR -> YUV (色彩域转换点)
void RegisterRgb2YuvMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_INT32;
    mod.out_type = DataPtrTypes::TYPE_INT32;

    mod.in_domain = ColorDomains::BGR;
    mod.out_domain = ColorDomains::YUV;

    mod.name = MOD_NAME;
    mod.run_function = Rgb2Yuv;

    RegisterIspModule(mod);
}