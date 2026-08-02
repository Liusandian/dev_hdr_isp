/**
 * @file yuv2rgb.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief
 * @version 0.1
 * @date 2023-07-27
 *
 * Copyright (c) of ADAS_EYES 2023
 *
 */
// =============================================================================
// 模块: yuv2rgb (YUV -> BGR 色彩空间转换)
// -----------------------------------------------------------------------------
// 功能: 将处理完成的 YUV 图像转换回 BGR 色彩空间, 用于最终显示或保存。
//
// 背景: YUV 域处理(伽马、对比度、锐化、降噪、饱和度)完成后, 显示器/编码器
//       需要 RGB 数据, 因此流水线末端需做 YUV->BGR 反变换。
//
// 转换公式(BT.601 逆变换, 8bit):
//   R = Y + 1.114*(V - 128)
//   G = Y - 0.395*(U - 128) - 0.581*(V - 128)
//   B = Y + 2.032*(U - 128)
//   (U/V 减 128 是因为色度以 128 为中心表示 0 色差)
// 数据流向: u8(YUV) -> u8(BGR), 色彩域由 YUV 转为 BGR。
// =============================================================================

#include "modules/modules.h"

#define MOD_NAME "yuv2rgb"

// YUV -> BGR 主函数
static int Yuv2Bgr(Frame *frame, const IspPrms *isp_prm)
{
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }
    int pixel_idx = 0;

    uint8_t *bgr_o = reinterpret_cast<uint8_t *>(frame->data.bgr_u8_o);   // BGR 输出(交叉存储 B/G/R)
    uint8_t *y_i = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_i.y);   // 输入亮度 Y
    uint8_t *u_i = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_i.u);   // 输入色度 U
    uint8_t *v_i = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_i.v);   // 输入色度 V

    FOR_ITER(ih, frame->info.height)
    {
        FOR_ITER(iw, frame->info.width)
        {
            int pixel_idx = GET_PIXEL_INDEX(iw, ih, frame->info.width);

            // BGR 输出引用(存储顺序 B, G, R)
            auto &b = bgr_o[3 * pixel_idx + 0];
            auto &g = bgr_o[3 * pixel_idx + 1];
            auto &r = bgr_o[3 * pixel_idx + 2];

            // BT.601 YUV -> RGB 逆变换
            // R = Y + 1.114*(V-128)
            int r_tmp = y_i[pixel_idx] + 0 + 1.114 * (v_i[pixel_idx] - 128);
            // G = Y - 0.395*(U-128) - 0.581*(V-128)
            int g_tmp = y_i[pixel_idx] - 0.395 * (u_i[pixel_idx] - 128) - 0.581 * (v_i[pixel_idx] - 128);
            // B = Y + 2.032*(U-128)
            int b_tmp = y_i[pixel_idx] + 2.032 * (u_i[pixel_idx] - 128) + 0;

            // 限幅到 [0, 255]
            ClipMinMax(r_tmp, 255, 0);
            ClipMinMax(g_tmp, 255, 0);
            ClipMinMax(b_tmp, 255, 0);

            r = static_cast<uint8_t>(r_tmp);
            g = static_cast<uint8_t>(g_tmp);
            b = static_cast<uint8_t>(b_tmp);
        }
    }

    return 0;
}

// 模块注册: s32 -> s32, YUV -> BGR (色彩域转换点)
void RegisterYuv2RgbMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_INT32;
    mod.out_type = DataPtrTypes::TYPE_INT32;

    mod.in_domain = ColorDomains::YUV;
    mod.out_domain = ColorDomains::BGR;

    mod.name = MOD_NAME;
    mod.run_function = Yuv2Bgr;

    RegisterIspModule(mod);
}