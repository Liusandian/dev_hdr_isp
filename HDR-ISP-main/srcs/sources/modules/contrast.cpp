/**
 * @file contrast.cpp
 * @author joker.mao (joker_mao@163.com)
 * @brief contrast improve
 * @version 0.1
 * @date 2023-08-12
 * Copyright (c) of ADAS_EYES 2023
 */
// =============================================================================
// 模块: contrast (对比度增强)
// -----------------------------------------------------------------------------
// 功能: 调整 Y 分量的对比度, 使亮部更亮、暗部更暗, 增强图像层次感。
//
// 算法(以 127 为中心的线性拉伸):
//   y' = y + (y - 127) * ratio
//   - ratio > 0: 增强对比度(偏离 127 的像素被推远)
//   - ratio < 0: 降低对比度(像素向 127 收敛)
//   - ratio = 0: 不变
// 以 127(8bit 中点)为参考点, 对称拉伸, 保持平均亮度不变。
// 数据流向: u8(Y) -> u8(Y), 色彩域保持 YUV。
// =============================================================================

#include "modules/modules.h"

#define MOD_NAME "contrast"

// 对比度增强主函数
static int Contrast(Frame *frame, const IspPrms *isp_prm)
{
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        LOG(ERROR) << "input prms is null";
        return -1;
    }
    int pixel_idx = 0;
    float contrast_ratio = isp_prm->contrast_prms.ratio;   // 对比度调整系数

    uint8_t *y_i = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_i.y);   // 输入亮度 Y
    uint8_t *y_o = reinterpret_cast<uint8_t *>(frame->data.yuv_u8_o.y);   // 输出亮度 Y

    FOR_ITER(h, frame->info.height)
    {
        FOR_ITER(w, frame->info.width)
        {
            pixel_idx = h * frame->info.width + w;

            int y = y_i[pixel_idx];
            // 以 127 为中心线性拉伸: y' = y + (y - 127) * ratio
            y = static_cast<int>(y + (y - 127) * contrast_ratio);
            // 限幅到 [0, 255]
            ClipMinMax(y, 255, 0);
            y_o[pixel_idx] = y;
        }
    }

    SwapMem<void>(frame->data.yuv_u8_i.y, frame->data.yuv_u8_o.y);

    return 0;
}

// 模块注册: s32 -> s32, YUV -> YUV
void RegisterContrastMod()
{
    IspModule mod;

    mod.in_type = DataPtrTypes::TYPE_INT32;
    mod.out_type = DataPtrTypes::TYPE_INT32;

    mod.in_domain = ColorDomains::YUV;
    mod.out_domain = ColorDomains::YUV;

    mod.name = MOD_NAME;
    mod.run_function = Contrast;

    RegisterIspModule(mod);
}