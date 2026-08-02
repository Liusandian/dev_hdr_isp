/**
 * @file demoasic.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief demoasic
 * @version 0.1
 * @date 2023-07-27
 * Copyright (c) of ADAS_EYES 2023
 */
// =============================================================================
// 模块: demoasic (去马赛克 / Demosaicing, 又称 Bayer 插值)
// -----------------------------------------------------------------------------
// 功能: 将单通道 Bayer RAW 图像插值为 3 通道 BGR 图像。
//
// 背景: 传感器每个像素只覆盖一种颜色的滤光片(R/G/B), 按 Bayer 排列交替分布,
//       因此每个像素只有 R/G/B 中的一个分量, 另外两个分量需通过邻域插值恢复。
//
// Bayer 排列(本模块支持两种常见模式, 由 CFA 参数决定):
//   RGGB:        BGGR:
//   R G R G      B Gb B Gb
//   G B G B      Gr R Gr R
//   R G R G      B Gb B Gb
//   G B G B      Gr R Gr R
//
// 插值策略(双线性插值, 简单高效):
//   设 3x3 邻域, 中心为 p4, 邻点 p0~p8:
//      p0 p1 p2
//      p3 p4 p5
//      p6 p7 p8
//   - R 像素位置: B = 4 角均值(p0+p2+p6+p8)/4, G = 4 边均值(p1+p3+p5+p7)/4
//   - B 像素位置: R = 4 角均值,               G = 4 边均值
//   - Gr/Gb 像素位置: 同行异色 = (p3+p5)/2,    同列异色 = (p1+p7)/2, G = p4
// 边缘像素(无法取邻域)直接复制自身到 3 通道。
// 数据流向: s32(RAW) -> s32(BGR), 色彩域由 RAW 转为 BGR。
// =============================================================================

#include "modules/modules.h"

#define MOD_NAME "demoasic"
/*
R G R G   B Gb B Gb
G B G B   Gr R Gr R
R G R G   B Gb B Gb
G B G B   Gr R Gr R
*/
// 去马赛克主函数
static int Demoasic(Frame *frame, const IspPrms *isp_prm)
{
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }
    int pixel_idx = 0;

    int32_t *raw32_in = reinterpret_cast<int32_t *>(frame->data.raw_s32_i);   // 单通道 Bayer RAW 输入
    int32_t *bgr_out = reinterpret_cast<int32_t *>(frame->data.bgr_s32_o);    // 3 通道 BGR 输出(交叉存储 B/G/R)

    FOR_ITER(h, frame->info.height)
    {
        FOR_ITER(w, frame->info.width)
        {
            pixel_idx = h * frame->info.width + w;

            // 边缘 1 像素无法构造 3x3 邻域, 直接用自身填充三通道
            if ((w < 1) || (h < 1) || (w > (frame->info.width - 2)) || (h > (frame->info.height - 2)))
            {
                bgr_out[pixel_idx * 3 + 2] = raw32_in[(h)*frame->info.width + w];
                bgr_out[pixel_idx * 3 + 1] = raw32_in[(h)*frame->info.width + w];
                bgr_out[pixel_idx * 3 + 0] = raw32_in[(h)*frame->info.width + w];
                continue;
            }

            // 取 3x3 邻域(注意: Bayer 排列下相邻像素为不同颜色)
            int32_t p0 = raw32_in[(h - 1) * frame->info.width + w - 1];  // 左上
            int32_t p1 = raw32_in[(h - 1) * frame->info.width + w];      // 上
            int32_t p2 = raw32_in[(h - 1) * frame->info.width + w + 1];  // 右上
            int32_t p3 = raw32_in[(h)*frame->info.width + w - 1];        // 左
            int32_t p4 = raw32_in[(h)*frame->info.width + w];            // 中心(当前像素)
            int32_t p5 = raw32_in[(h)*frame->info.width + w + 1];        // 右
            int32_t p6 = raw32_in[(h + 1) * frame->info.width + w - 1];  // 左下
            int32_t p7 = raw32_in[(h + 1) * frame->info.width + w];      // 下
            int32_t p8 = raw32_in[(h + 1) * frame->info.width + w + 1];  // 右下

            int cfa_id = static_cast<int>(frame->info.cfa);
            switch (kPixelCfaLut[cfa_id][w % 2][h % 2])
            {
            case PixelCfaTypes::R:
                // R 像素位置: B 用 4 角均值, G 用 4 边均值, R 取自身
                bgr_out[pixel_idx * 3 + 0] = (p0 + p2 + p6 + p8) >> 2; // B = 4 角均值
                bgr_out[pixel_idx * 3 + 1] = (p1 + p3 + p5 + p7) >> 2; // G = 4 边均值
                bgr_out[pixel_idx * 3 + 2] = p4;                       // R = 中心
                break;
            case PixelCfaTypes::GR:
                // Gr 像素位置(行内左右为 B, 上下为 R):
                //   B = 左右两邻均值, R = 上下两邻均值, G = 中心
                bgr_out[pixel_idx * 3 + 0] = (p3 + p5) >> 1;
                bgr_out[pixel_idx * 3 + 1] = p4;
                bgr_out[pixel_idx * 3 + 2] = (p1 + p7) >> 1;
                break;
            case PixelCfaTypes::GB:
                // Gb 像素位置(行内左右为 R, 上下为 B):
                //   B = 上下两邻均值, R = 左右两邻均值, G = 中心
                bgr_out[pixel_idx * 3 + 0] = (p1 + p7) >> 1;
                bgr_out[pixel_idx * 3 + 1] = p4;
                bgr_out[pixel_idx * 3 + 2] = (p3 + p5) >> 1;
                break;
            case PixelCfaTypes::B:
                // B 像素位置: R 用 4 角均值, G 用 4 边均值, B 取自身
                bgr_out[pixel_idx * 3 + 0] = p4;                       // B = 中心
                bgr_out[pixel_idx * 3 + 1] = (p1 + p3 + p5 + p7) >> 2; // G = 4 边均值
                bgr_out[pixel_idx * 3 + 2] = (p0 + p2 + p6 + p8) >> 2; // R = 4 角均值
                break;
            default:
                break;
            }

            // 限幅, 防止插值溢出
            ClipMinMax<int32_t>(bgr_out[pixel_idx * 3 + 0], (int32_t)isp_prm->info.max_val, 0);
            ClipMinMax<int32_t>(bgr_out[pixel_idx * 3 + 1], (int32_t)isp_prm->info.max_val, 0);
            ClipMinMax<int32_t>(bgr_out[pixel_idx * 3 + 2], (int32_t)isp_prm->info.max_val, 0);
        }
    }

    SwapMem<void>((frame->data.bgr_s32_o), (frame->data.bgr_s32_i));

    return 0;
}

// 模块注册: s32 -> s32, RAW -> BGR (色彩域转换点)
void RegisterDemoasicMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_INT32;
    mod.out_type = DataPtrTypes::TYPE_INT32;

    mod.in_domain = ColorDomains::RAW;
    mod.out_domain = ColorDomains::BGR;

    mod.name = MOD_NAME;
    mod.run_function = Demoasic;

    RegisterIspModule(mod);
}